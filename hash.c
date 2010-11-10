#include "obsfs.h"
#include "hash.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
  char *path;
  struct stat st;
} attr_t;

attr_t attr_hash[ATTR_CACHE_SIZE];
dir_t dir_hash[DIR_CACHE_SIZE];

static uint32_t hash_string(const char *str)
{
  uint32_t c = 0;
  while (*str) {
    c = (c << 8) | ((c >> 24) ^ *str);
    str++;
  }
  return c;
}

void hash_init(void)
{
  memset(attr_hash, 0, sizeof(attr_hash));
}

void hash_add_attr(const char *path, struct stat *st)
{
  attr_t *h = &attr_hash[hash_string(path) % ATTR_CACHE_SIZE];
  if (h->path)
    free(h->path);
  h->path = strdup(path);
  h->st = *st;
}

struct stat *hash_find_attr(const char *path)
{
  attr_t *h = &attr_hash[hash_string(path) % ATTR_CACHE_SIZE];
  if (h->path) {
    if (strcmp(path, h->path))
      return NULL;
    else
      return &h->st;
  }
  else
    return NULL;
}

void hash_free(void)
{
  int i;
  for (i = 0; i < ATTR_CACHE_SIZE; i++) {
    if (attr_hash[i].path)
      free(attr_hash[i].path);
  }
}

void dir_cache_init(void)
{
  memset(dir_hash, 0, sizeof(dir_t) * DIR_CACHE_SIZE);
}

static void free_dir(dir_t *d)
{
  int i;
  if (d->path) {
    free(d->path);
    if (d->entries) {
      for (i = 0; i < d->num_entries; i++) {
        free(d->entries[i].name);
      }
      free(d->entries);
    }
  }
}

dir_t *dir_new(const char *path)
{
  dir_t *d = &dir_hash[hash_string(path) % DIR_CACHE_SIZE];
  free_dir(d);
  d->path = strdup(path);
  d->entries = NULL;
  d->num_entries = 0;
  return d;
}

void dir_add(dir_t *dir, const char *name, int is_dir)
{
  dirent_t *de;
  dir->entries = realloc(dir->entries, (dir->num_entries + 1) * sizeof(dirent_t));
  de = &dir->entries[dir->num_entries];
  de->name = strdup(name);
  de->is_dir = is_dir;
  dir->num_entries++;
}

int dir_find(dirent_t **dir, const char *path)
{
  dir_t *d = &dir_hash[hash_string(path) % DIR_CACHE_SIZE];
  if (!d->path)
    return -1;
  *dir = d->entries;
  return d->num_entries;
}

void dir_cache_free(void)
{
  int i;
  for (i = 0; i < DIR_CACHE_SIZE; i++) {
    dir_t *d = &dir_hash[i];
    free_dir(d);
  }
}
