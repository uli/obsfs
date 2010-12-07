/*
 * util.c
 * (c) 2010 Ulrich Hecht, SuSE Linux Products GmbH <uli@suse.de>
 *
 * This file is part of obsfs.
 *
 * obsfs is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 2 or version 3 of the License.
 *
 * obsfs is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with obsfs.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

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

char *make_url(const char *url_prefix, const char *path, const char *rev)
{
  char *urlbuf = malloc(strlen(url_prefix) + strlen(path) + strlen("?rev=") + 32 + 1);
  sprintf(urlbuf, "%s%s%s%s", url_prefix, path, rev? "?rev=" : "", rev? : "");
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

int endswith(const char *str, const char *end)
{
  if (strlen(str) < strlen(end))
    return 0;
  else
    return !strcmp(str + strlen(str) - strlen(end), end);
}

/* extensions associated with files */
static const char *file_exts[] = {
  ".rpm", ".repo", ".xml", ".gz", ".key", ".asc", ".solv", NULL
};

/* names that indicate a file if below a given directory tree */
static const char *path_name[][2] = {
  { "/published/", "content" },
  { "/published/", "packages" },
  { "/published/", "packages.DU" },
  { "/published/", "packages.en" },
  { "/published/", "directory.yast" },
  { NULL, NULL }
};

/* directories that exclusively contain files */
static const char *file_only_dirs[] = {
  "/repocache", NULL
};

/* Is the entry "filename" in (API) directory "path" a file? */
int is_a_file(const char *path, const char *filename)
{
  const char **e;
  for (e = file_exts; *e; e++) {
    if (endswith(filename, *e))
      return 1;
  }
  
  const char *(*pn)[2];	/* C syntax rulez... NOT! */
  for (pn = path_name; (*pn)[0]; pn++) {
    if (!strncmp((*pn)[0], path, strlen((*pn)[0])) && !strcmp((*pn)[1], filename))
      return 1;
  }
  
  for (e = file_only_dirs; *e; e++) {
    if (endswith(path, *e))
      return 1;
  }
  
  return 0;
}
