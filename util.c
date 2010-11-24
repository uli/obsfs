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

