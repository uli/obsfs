#define FUSE_USE_VERSION  26

#include <fuse.h>
#include <fuse/fuse_opt.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <curl/curl.h>
#include <expat.h>
#include <libgen.h>
#include <unistd.h>

#include "obsfs.h"
#include "cache.h"

/* leftovers from the Hello, World example */
static const char *hello_str = "Hello World!\n";
static const char *hello_path = "/hello";

char *file_cache_dir = NULL;	/* directory to keep cached file contents in */
int file_cache_count = 1;	/* used to make up names for cached files */

char *url_prefix = NULL;	/* prefix for API calls (https://...) */

/* filesystem options */
struct options {
  char *api_username;	/* API user name */
  char *api_password;	/* API user password */
  char *api_hostname;	/* API server name */
} options;

/* lifted from the Hello, World with options example */
#define OBSFS_OPT_KEY(t, p, v) { t, offsetof(struct options, p), v }

static struct fuse_opt obsfs_opts[] =
{
  OBSFS_OPT_KEY("user=%s", api_username, 0),
  OBSFS_OPT_KEY("pass=%s", api_password, 0),
  OBSFS_OPT_KEY("host=%s", api_hostname, 0),
  FUSE_OPT_END
};

static int obsfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi);
                         
static int obsfs_getattr(const char *path, struct stat *stbuf)
{
  /* initialize the stat buffer we are going to fill in */
  memset(stbuf, 0, sizeof(struct stat));
  
  if (strcmp(path, "/") == 0 || strcmp(path, "/build") == 0) {
    /* root and the stuff inside it cannot be deduced because the server
       returns a human-readable info page for "/", so they are hardcoded
       here. */
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else if (strcmp(path, hello_path) == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = strlen(hello_str);
  } else {
    /* actual API files and directories */
    struct stat *ret;
    fprintf(stderr, "getattr: looking for %s\n", path);
    /* let's see if we have that cached already */
    ret = attr_cache_find(path);
    if (ret) {
      fprintf(stderr, "found it!\n");
      *stbuf = *ret;
    } 
    else {
      /* Cache miss, we are going to retrieve the directory "path" is in.
         The reason for that is that the only way to find out about
         a directory entry is to retrieve the entire directory from the
         server. We thus have obsfs_readdir() obtain the directory
         without giving it a filler function, so it will only cache
         the entries it finds in the attribute cache, where we can
         subsequently retrieve the one we're looking for. */
      /* FIXME: Fails if there is a hash collision between the file we're
         looking for and another one in the same directory, and the latter 
         comes last. */
      char *dir = strdup(path);	/* dirname modifies its argument, need to copy */
      fprintf(stderr, "not found, trying to get directory\n");
      /* call with buf and filler NULL for cache-only operation */
      obsfs_readdir(dirname(dir), NULL, NULL, 0, NULL);
      free(dir);
      /* now the attributes are in the attr cache (if it exists at all) */
      ret = attr_cache_find(path);
      if (ret) {
        fprintf(stderr, "found it after all\n");
        *stbuf = *ret;
      }
      else {
        /* file not found */
        /* FIXME: should we still have this fallback? */
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
      }
    }
  }

  return 0;
}

/* data we need in the expat callbacks to save the directory entries */
struct filbuf {
  void *buf;			/* directory entry buffer, provided by FUSE */
  fuse_fill_dir_t filler;	/* buffer filler function */
  const char *path;		/* directory to read */
  dir_t *cdir;			/* dir cache entry to fill in */
  int in_dir;			/* flag set when inside a <directory> or <binarylist> */
};

/* expat tag start callback for reading API directories */
static void expat_api_dir_start(void *ud, const XML_Char *name, const XML_Char **atts)
{
  struct stat st;
  struct filbuf *fb = (struct filbuf *)ud;

  memset(&st, 0, sizeof(struct stat));

  /* start of directory */
  if (!strcmp(name, "directory") || !strcmp(name, "binarylist")) {
    fb->in_dir = 1;
    return;
  }
  
  /* directory entry */
  if (fb->in_dir && (!strcmp(name, "entry") || !strcmp(name, "binary"))) {
    const char *filename = NULL;
    
    /* process all attributes */
    while (*atts) {
      if (!strcmp(atts[0], "name")) {
        /* entry in a "directory" directory; we assume it is itself a directory */
        filename = atts[1];
        st.st_mode = S_IFDIR;
      }
      else if (!strcmp(atts[0], "filename")) {
        /* entry in a "binarylist" directory, this is always a regular file */
        filename = atts[1];
        st.st_mode = S_IFREG;
      }
      else if (!strcmp(atts[0], "size")) {
        /* file size */
        st.st_size = atoi(atts[1]);
      }
      atts += 2; /* expat hands us a string array with name/value pairs */
    }
    if (filename) {
      char *full_path;	/* for the attribute cache, we need the full path */
      /* fb->filler might be NULL if we're called from obsfs_getattr() for the
         task of filling the attribute cache */
      if (fb->filler) {
        fb->filler(fb->buf, filename, &st, 0);
      }
      /* add this entry to the directory cache */
      dir_cache_add(fb->cdir, filename, st.st_mode == S_IFDIR);

      /* add this entry to the attribute cache */
      full_path = malloc(strlen(fb->path) + strlen(filename) + 2);
      sprintf(full_path, "%s/%s", fb->path, filename);
      fprintf(stderr, "hashing %s with size %ld\n", full_path, st.st_size);
      attr_cache_add(full_path, &st);
      free(full_path);
    }
  }
}

/* expat tag end handler for reading API directories */
static void expat_api_dir_end(void *ud, const XML_Char *name)
{
  struct filbuf *fb = (struct filbuf *)ud;
  /* end of API directory */
  if (!strcmp(name, "directory") || !strcmp(name, "binarylist")) {
    fb->in_dir = 0;
  }
}

/* expat expects an fwrite()-style callback, so we need an adapter for XML_Parse() */
static size_t write_adapter(void *ptr, size_t size, size_t nmemb, void *userdata)
{
  XML_Parse((XML_Parser)userdata, ptr, size * nmemb, 0);
  return size * nmemb;
}

/* initialize curl and set API user name and password, writer function and user data */
static CURL *curl_open_file(const char *url, void *write_fun, void *user_data)
{
  CURL *curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_USERNAME, options.api_username);
  curl_easy_setopt(curl, CURLOPT_PASSWORD, options.api_password);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_fun);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, user_data);
  return curl;
}

/* find the depth of a path (used to determine whether it's necessary
   to add additional "fixed" nodes such as _log, _status etc. */
static int path_depth(const char *path)
{
  int count = 0;
  for (path++ /* skip first slash */; *path; path++) {
    if (*path == '/')
      count++;
  }
  return count;
}

/* read an API directory and fill in the FUSE directory buffer, the directory
   cache, and the attribute cache */
static int get_api_dir(const char *path, void *buf, fuse_fill_dir_t filler)
{
  char *urlbuf;	/* used to compose the full API URL */
  XML_Parser xp;
  CURL *curl;
  CURLcode ret;
  struct filbuf fb;	/* data the expat callbacks need */
  dirent_t *cached_dirents;
  int cached_dirents_size;

  /* see if we have this directory cached already */
  cached_dirents_size = dir_cache_find(&cached_dirents, path);
  if (cached_dirents_size != -1) {
    /* cache hit */
    int i;
    struct stat st;

    /* since this dir is already cached, we are done if we don't have a filler */
    if (!filler)
      return 0;

    /* fill the FUSE dir buffer with our cached entries */
    memset(&st, 0, sizeof(struct stat));
    for (i = 0; i < cached_dirents_size; i++) {
      if (cached_dirents[i].is_dir)
        st.st_mode = S_IFDIR;
      else
        st.st_mode = S_IFREG;
      filler(buf, cached_dirents[i].name, &st, 0);
    }
  }
  else {
    /* not in cache, we have to retrieve it from the API server */
    dir_t *newdir = dir_cache_new(path); /* get directory cache handle */

    xp = XML_ParserCreate(NULL);   /* create an expat parser */
    if (!xp)
      return 1;

    /* copy some data that the parser callbacks will need */
    fb.filler = filler;
    fb.buf = buf;
    fb.path = path;
    fb.cdir = newdir;
    fb.in_dir = 0;
    XML_SetUserData(xp, (void *)&fb);	/* pass the data to the parser */

    /* set handlers for start and end tags */
    XML_SetElementHandler(xp, expat_api_dir_start, expat_api_dir_end);
    
    /* construct the full API URL for this directory */
    urlbuf = malloc(strlen(url_prefix) + strlen(path) + 1);
    sprintf(urlbuf, "%s%s", url_prefix, path);
    
    /* open the URL and set up CURL options */
    curl = curl_open_file(urlbuf, write_adapter, xp);
    //fprintf(stderr, "username %s pw %s\n", options.api_username, options.api_password);
    
    /* perform the actual retrieval; this will instruct curl to get the data from
       the API server and call the write_adapter() for each hunk of data, which will
       in turn call XML_Parse() which will funnel the invidiual components through
       the start and end tag handlers expat_api_dir_start() and expat_api_dir_end() */
    if ((ret = curl_easy_perform(curl))) {
      fprintf(stderr,"curl error %d\n", ret);
    }
    
    /* clean up stuff */
    curl_easy_cleanup(curl);
    XML_ParserFree(xp);
    free(urlbuf);
    
    /* check if we need to add additional nodes */
    /* Most of the available API is not exposed through directories. We have to know
       about it and add them ourselves at the appropriate places. */
    fprintf(stderr, "path depth of %s is %d\n", path, path_depth(path));
    
    /* log, history, status, and reason for packages */
    if (!strncmp("/build", path, 6) && path_depth(path) == 4) {
      int i;
      struct stat st;
      char *full_path;

      memset(&st, 0, sizeof(struct stat));
      st.st_mode = S_IFREG;
      /* st.st_size = 4096; not sure if this is a good idea */
      
      /* package status APIs */
      const char const *status_api[] = {
        "_history", "_reason", "_status", "_log", NULL
      };
      for (i = 0; status_api[i]; i++) {
        /* add node to the directory buffer (if any) */
        if (filler)
          filler(buf, status_api[i], &st, 0);

        /* compose a full path and add node to the attribute cache */
        full_path = malloc(strlen(path) + 1 /* slash */ + strlen(status_api[i]) + 1 /* null */);
        sprintf(full_path, "%s/%s", path, status_api[i]);
        attr_cache_add(full_path, &st);
        
        /* add node to the directory cache entry */
        dir_cache_add(newdir, status_api[i], 0);
        
        free(full_path);
      }
    }
  }
  return 0;
}

static int obsfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
  (void)offset;
  (void)fi;
  struct stat st;

  memset(&st, 0, sizeof(st));
  fprintf(stderr, "readdir path %s\n", path);
  
  /* The API server does not provide us with a root directory; retrieving
     "/" only yields a human-readable info page. We therefore have to
     construct the root directory manually. */
  if (!strcmp(path, "/") && filler && buf) {
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, hello_path + 1, NULL, 0);
    st.st_mode = S_IFDIR;
    filler(buf, "build", &st, 0);
  }
  
  /* If it's not the root directory, we get it from the API server (or dir cache). */
  return get_api_dir(path, buf, filler);
}

/* retrieve a file, store it in our local file cache, and return a descriptor
   to the local copy */
static int obsfs_open(const char *path, struct fuse_file_info *fi)
{
  char *urlbuf;
  CURL *curl;
  FILE *fp;
  char filename[11];
  CURLcode ret;
  struct stat st;
  
  /* leftovers from Hello, World example */
  if (strcmp(path, hello_path) == 0) {
    if ((fi->flags & 3) != O_RDONLY)
      return -EACCES;
    fi->fh = 0;
    return 0;
  }

  /* make up a file name for the cached file; it doesn't really matter what
     it looks like, we unlink() it after creation anyway */
  /* FIXME: racy, all threads share the same temp directory */
  sprintf(filename, "%d", file_cache_count++);

  /* create the cache file */
  fp = fopen(filename, "w+");
  unlink(filename);
  
  /* compose the full URL */
  urlbuf = malloc(strlen(url_prefix) + strlen(path) + 1);
  sprintf(urlbuf, "%s%s", url_prefix, path);
  
  /* retrieve the file from the API server */
  curl = curl_open_file(urlbuf, fwrite, fp);
  if ((ret = curl_easy_perform(curl))) {
    fprintf(stderr,"curl error %d\n", ret);
  }
  curl_easy_cleanup(curl);
  
  free(urlbuf);
  /* create a new file handle for the cache file, we need it later to retrieve
     the contents (it's unlinked already) */
  fi->fh = dup(fileno(fp));
  fclose(fp);

  /* now that we have the actual size, update the stat cache; this is necessary
     for the special nodes, the sizes of which we don't know when constructing
     their directory entries */
  if (fstat(fi->fh, &st)) {
    perror("fstat");
  }
  attr_cache_add(path, &st);

  return 0;
}

static int obsfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
  size_t len;
  /* Hello, World leftover */
  if (strcmp(path, hello_path) == 0) {
    len = strlen(hello_str);
    if (offset < len) {
      if (offset + size > len)
        size = len - offset;
      memcpy(buf, hello_str + offset, size);
    } else
      size = 0;

    return size;
  }
  
  /* read from the cache file */
  return pread(fi->fh, buf, size, offset);
}

static void *obsfs_init(struct fuse_conn_info *conn)
{
  /* change to the file cache directory; that way we don't have to remember it elsewhere */
  if (chdir(file_cache_dir)) {
    perror("chdir");
    abort();
  }

  /* construct an URL prefix from API server host name, user name and password */
  const char *host;
  if (options.api_hostname)
    host = options.api_hostname;
  else
    host = "api.opensuse.org";
  url_prefix = malloc(strlen(host) + strlen("https://") + 1);
  sprintf(url_prefix, "https://%s", host);

  return NULL;
}

static void obsfs_destroy(void *foo)
{
  free(url_prefix);
}

static struct fuse_operations obsfs_oper = {
  .init = obsfs_init,
  .destroy = obsfs_destroy,
  .getattr = obsfs_getattr,
  .readdir = obsfs_readdir,
  .open = obsfs_open,
  .read = obsfs_read,
};

int main(int argc, char *argv[])
{
  int ret;

  /* parse filesystem options */
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  memset(&options, 0, sizeof(struct options));
  if (fuse_opt_parse(&args, &options, obsfs_opts, NULL) == -1)
    return -1;

  /* initialize libcurl */
  if (curl_global_init(CURL_GLOBAL_ALL))
    return -1;

  /* initialize caches */
  attr_cache_init();
  dir_cache_init();
  
  /* create a directory for the file cache */
  file_cache_dir = strdup("/tmp/obsfs_cacheXXXXXX");
  if (!mkdtemp(file_cache_dir)) {
    perror("mkdtemp");
    return -1;
  }
  
  /* Go! */
  ret = fuse_main(args.argc, args.argv, &obsfs_oper, NULL);
  
  /* remove the file cache; no need to delete any files because they are all
     unlinked immediately after creation */
  if (rmdir(file_cache_dir)) {
    perror("rmdir");
  }
  free(file_cache_dir);
  
  fuse_opt_free_args(&args);
  attr_cache_free();
  dir_cache_free();
  
  return ret;
}
