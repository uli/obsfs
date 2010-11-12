#include "obsfs.h"
#include "cache.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
  char *path;
  struct stat st;
} attr_t;

attr_t attr_hash[ATTR_CACHE_SIZE];
dir_t dir_hash[DIR_CACHE_SIZE];

/* simplistic string hashing function */
static uint32_t hash_string(const char *str)
{
  uint32_t c = 0;
  while (*str) {
    c = (c << 8) | ((c >> 24) ^ *str);
    str++;
  }
  return c;
}

/* clear attribute cache */
void hash_init(void)
{
  memset(attr_hash, 0, sizeof(attr_hash));
}

/* add an entry to the attribute cache */
void hash_add_attr(const char *path, struct stat *st)
{
  attr_t *h = &attr_hash[hash_string(path) % ATTR_CACHE_SIZE];

  /* we don't care about collisions, but we need to free() old entries if any */
  if (h->path)
    free(h->path);

  h->path = strdup(path);
  h->st = *st;
}

/* retrieve an entry from the attribute cache */
struct stat *hash_find_attr(const char *path)
{
  attr_t *h = &attr_hash[hash_string(path) % ATTR_CACHE_SIZE];
  if (h->path) {
    /* is this actually the entry we're looking for, or just one with the same hash? */
    if (strcmp(path, h->path))
      return NULL;	/* cache miss */
    else
      return &h->st;	/* cache hit */
  }
  else
    return NULL;	/* cache miss */
}

/* free() memory used by attribute cache entries */
void hash_free(void)
{
  int i;
  for (i = 0; i < ATTR_CACHE_SIZE; i++) {
    if (attr_hash[i].path)
      free(attr_hash[i].path);
  }
  /* FIXME: shouldn't we clear the array here? */
}

/* clear directory cache */
void dir_cache_init(void)
{
  memset(dir_hash, 0, sizeof(dir_t) * DIR_CACHE_SIZE);
}

/* free() the memory occupied by a directory cache entry (if any) */
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

/* create a new directory cache entry */
dir_t *dir_new(const char *path)
{
  dir_t *d = &dir_hash[hash_string(path) % DIR_CACHE_SIZE];

  /* we don't care about collisions, but we need to free() an old entry there is one */
  free_dir(d);

  d->path = strdup(path);
  d->entries = NULL;
  d->num_entries = 0;
  return d;
}

/* add a node to a directory cache entry */
void dir_add(dir_t *dir, const char *name, int is_dir)
{
  dirent_t *de;
  /* allocate memory for one more node */
  dir->entries = realloc(dir->entries, (dir->num_entries + 1) * sizeof(dirent_t));
  de = &dir->entries[dir->num_entries];	/* pointer to the last node */
  de->name = strdup(name);
  de->is_dir = is_dir;
  dir->num_entries++;
}

/* retrieve a directory cache entry */
int dir_find(dirent_t **dir, const char *path)
{
  dir_t *d = &dir_hash[hash_string(path) % DIR_CACHE_SIZE];
  if (!d->path || strcmp(d->path, path))
    return -1;	/* cache miss */
  *dir = d->entries;
  return d->num_entries;
}

/* free() memory used by directory cache entries */
void dir_cache_free(void)
{
  int i;
  for (i = 0; i < DIR_CACHE_SIZE; i++) {
    free_dir(&dir_hash[i]);
  }
  /* FIXME: shouldn't we clear the array here? */
}
