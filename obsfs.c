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
#include <search.h>
#include <libgen.h>

static const char *hello_str = "Hello World!\n";
static const char *hello_path = "/hello";

struct options {
  char *api_username;
  char *api_password;
} options;

#define ATTR_CACHE_SIZE 32768
struct hsearch_data attr_cache;

#define OBSFS_OPT_KEY(t, p, v) { t, offsetof(struct options, p), v }

static struct fuse_opt obsfs_opts[] =
{
  OBSFS_OPT_KEY("apiuser=%s", api_username, 0),
  OBSFS_OPT_KEY("apipass=%s", api_password, 0),
  FUSE_OPT_END
};

static int obsfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi);
                         
static int obsfs_getattr(const char *path, struct stat *stbuf)
{
  int res = 0;
  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0 || strcmp(path, "/build") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else if (strcmp(path, hello_path) == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = strlen(hello_str);
  } else {
    //res = -ENOENT;
    ENTRY e;
    ENTRY *ret;
    e.key = path;
    fprintf(stderr, "getattr: looking for %s\n", e.key);
    hsearch_r(e, FIND, &ret, &attr_cache);
    if (ret) {
      fprintf(stderr, "found it!\n");
      *stbuf = *(struct stat *)(ret->data);
    } 
    else {
      char *dir = strdup(path);
      fprintf(stderr, "not found, trying to get directory\n");
      obsfs_readdir(dirname(dir), NULL, NULL, 0, NULL);
      free(dir);
      hsearch_r(e, FIND, &ret, &attr_cache);
      if (ret) {
        fprintf(stderr, "found it after all\n");
        *stbuf = *(struct stat *)(ret->data);
      }
      else {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
      }
    }
  }

  return res;
}

static int in_api_dir = 0;
struct filbuf {
  void *buf;
  fuse_fill_dir_t filler;
  const char *path;
};

static void expat_api_dir_start(void *ud, const XML_Char *name, const XML_Char **atts)
{
  struct stat st;
  struct filbuf *fb = (struct filbuf *)ud;
  memset(&st, 0, sizeof(struct stat));
  st.st_mode = S_IFDIR;
  if (!strcmp(name, "directory") || !strcmp(name, "binarylist")) {
    in_api_dir = 1;
    return;
  }
  if (in_api_dir && (!strcmp(name, "entry") || !strcmp(name, "binary"))) {
    while (*atts) {
      char *filename = NULL;
      if (!strcmp(atts[0], "name")) {
        filename = atts[1];
        st.st_mode = S_IFDIR;
      }
      else if (!strcmp(atts[0], "filename")) {
        filename = atts[1];
        st.st_mode = S_IFREG;
      }
      else if (!strcmp(atts[0], "size")) {
        st.st_size = atoi(atts[1]);
      }
      if (filename) {
        ENTRY e;
        ENTRY *re;
        if (fb->filler)
          fb->filler(fb->buf, filename, &st, 0);
        /* woohoo, memleaks! */
        e.key = malloc(strlen(fb->path) + strlen(filename) + 2);
        sprintf(e.key, "%s/%s", fb->path, filename);
        e.data = malloc(sizeof(struct stat)); *((struct stat *)e.data) = st;
        fprintf(stderr, "hashing %s\n", e.key);
        hsearch_r(e, ENTER, &re, &attr_cache);
      }
      atts += 2;
    }
  }
}

static void expat_api_dir_end(void *ud, const XML_Char *name)
{
  if (!strcmp(name, "directory")) {
    in_api_dir = 0;
  }
}

static size_t write_adapter(void *ptr, size_t size, size_t nmemb, void *userdata)
{
  XML_Parse((XML_Parser)userdata, ptr, size * nmemb, 0);
  return size * nmemb;
}

static CURL *curl_open_file(const char *url)
{
  CURL *curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_USERNAME, options.api_username);
  curl_easy_setopt(curl, CURLOPT_PASSWORD, options.api_password);
  return curl;
}

static int get_api_dir(const char *path, void *buf, fuse_fill_dir_t filler)
{
  const char *prefix = "https://api.opensuse.org";
  char *urlbuf;
  XML_Parser xp;
  CURL *curl;
  CURLcode ret;
  struct filbuf fb;
  xp = XML_ParserCreate(NULL);
  fb.filler = filler;
  fb.buf = buf;
  fb.path = path;
  XML_SetUserData(xp, (void *)&fb);
  if (!xp)
    return 1;
  XML_SetElementHandler(xp, expat_api_dir_start, expat_api_dir_end);
  urlbuf = malloc(strlen(prefix) + strlen(path) + 1);
  sprintf(urlbuf, "%s%s", prefix, path);
  curl = curl_open_file(urlbuf);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_adapter);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, xp);
  fprintf(stderr, "username %s pw %s\n", options.api_username, options.api_password);
  if ((ret = curl_easy_perform(curl))) {
    fprintf(stderr,"curl error %d\n", ret);
  }
  curl_easy_cleanup(curl);
  XML_ParserFree(xp);
  free(urlbuf);
  return 0;
}

static int obsfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
  (void)offset;
  (void)fi;
  struct stat st;
  fprintf(stderr, "readdir path %s\n", path);
  
  if (!strcmp(path, "/") && filler && buf) {
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, hello_path + 1, NULL, 0);
    st.st_mode = S_IFDIR;
    filler(buf, "build", &st, 0);
  }
  
  return get_api_dir(path, buf, filler);
}

static int obsfs_open(const char *path, struct fuse_file_info *fi)
{
  if (strcmp(path, hello_path) != 0)
    return -ENOENT;

  if ((fi->flags & 3) != O_RDONLY)
    return -EACCES;

  return 0;
}

static int obsfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
  size_t len;
  (void)fi;
  if (strcmp(path, hello_path) != 0)
    return -ENOENT;

  len = strlen(hello_str);
  if (offset < len) {
    if (offset + size > len)
      size = len - offset;
    memcpy(buf, hello_str + offset, size);
  } else
    size = 0;

  return size;
}

static struct fuse_operations obsfs_oper = {
  .getattr = obsfs_getattr,
  .readdir = obsfs_readdir,
  .open = obsfs_open,
  .read = obsfs_read,
};

int main(int argc, char *argv[])
{
  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  memset(&options, 0, sizeof(struct options));
  
  if (fuse_opt_parse(&args, &options, obsfs_opts, NULL) == -1)
    return -1;

  if (curl_global_init(CURL_GLOBAL_ALL))
    return -1;

  if (!hcreate_r(ATTR_CACHE_SIZE, &attr_cache)) {
    perror("hcreate_r");
    return -1;
  }
  
  ret = fuse_main(args.argc, args.argv, &obsfs_oper, NULL);
  
  fuse_opt_free_args(&args);
  hdestroy_r(&attr_cache);
  
  return ret;
}
