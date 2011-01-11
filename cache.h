/*
 * cache.h
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

#include <sys/stat.h>
#include "uthash.h"

typedef struct {
  char *path;
  struct stat st;
  char *symlink;
  char *hardlink;
  time_t timestamp;
  int modified;
  char *rev;	/* build service revision */
  UT_hash_handle hh;
} attr_t;

/* one node of a directory cache entry */
typedef struct {
  char *name;
  int is_dir;
} dirent_t;

/* directory cache entry */
typedef struct {
  char *path;
  dirent_t *entries;
  int num_entries;
  time_t timestamp;
  int modified;
  char *rev; /* build service revision */
  UT_hash_handle hh;
} dir_t;

/* attribute cache methods */
void attr_cache_init(void);
void attr_cache_add(const char *path, struct stat *st, const char *symlink, const char *hardlink, const char *rev);
attr_t *attr_cache_find(const char *path);
void attr_cache_free(void);
void attr_cache_remove(const char *path);

/* directory cache methods */
void dir_cache_init(void);
dir_t *dir_cache_new(const char *path);
void dir_cache_add(dir_t *dir, const char *name, int is_dir);
void dir_cache_remove(const char *path);
void dir_cache_add_dir_by_name(const char *path);
dir_t *dir_cache_find(const char *path);
void dir_cache_free(void);
