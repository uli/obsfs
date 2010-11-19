#include "util.h"

#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int mkdirp(const char *pathname, mode_t mode)
{
  char *dname = dirname(strdup(pathname));
  fprintf(stderr, "MKDIRP trying to create directory %s\n", dname);
  if (mkdir(dname, mode)) {
    if (errno == EEXIST) {
      free(dname);
      return 0;
    }
    else if (errno == ENOENT) {
      if (mkdirp(dname, mode)) {
        goto error;
      }
      return mkdir(dname, mode);
    }
    else
      goto error;
  }
  else {
    free(dname);
    return 0;
  }
error:
  free(dname);
  return -1;
}

char *dirname_c(const char *path, char **basenm)
{
  char *p = strdup(path);
  if (basenm)
    *basenm = basename(p);
  return dirname(p);
}

char *make_url(const char *url_prefix, const char *path)
{
  char *urlbuf = malloc(strlen(url_prefix) + strlen(path) + 1);
  sprintf(urlbuf, "%s%s", url_prefix, path);
  return urlbuf;
}

char *get_match(regmatch_t match, const char *str)
{
  int len = match.rm_eo - match.rm_so;
  char *p = malloc(len + 1);
  memcpy(p, str + match.rm_so, len);
  p[len] = 0;
  return p;
}

