#include "obsfs.h"
#include "cache.h"

#define CACHE_DEBUG

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifdef CACHE_DEBUG
#define DEBUG(x...) fprintf(stderr, x)
#else
#define DEBUG(x...)
#endif

attr_t *attr_hash;
dir_t *dir_hash;

/* clear attribute cache */
void attr_cache_init(void)
{
  attr_hash = NULL;
}

static void free_attr(attr_t *h)
{
    free(h->path);
    if (h->symlink)
      free(h->symlink);
    if (h->hardlink)
      free(h->hardlink);
    free(h);
}

/* add an entry to the attribute cache */
void attr_cache_add(const char *path, struct stat *st, const char *symlink, const char *hardlink)
{
  attr_t *h = calloc(1, sizeof(attr_t));

  /* create the new entry; do this before deleting the old one because symlink
     and hardlink may point to it */
  h->path = strdup(path);
  h->st = *st;
  if (symlink)
    h->symlink = strdup(symlink);
  if (hardlink)
    h->hardlink = strdup(hardlink);
  h->timestamp = time(NULL);
  
  /* need to delete old entry, if any */
  attr_t *old;
  HASH_FIND_STR(attr_hash, path, old);
  if (old) {
    DEBUG("ATTR CACHE: found old entry for %s\n", path);
    HASH_DEL(attr_hash, old);
    free_attr(old);
  }
  
  /* can't use the HASH_ADD_STR() convenience macro here because it
     expects the key to be an array inside the hash structure, not
     a pointer somewhere else; HASH_FIND_STR() works fine, though. */
  HASH_ADD_KEYPTR(hh, attr_hash, h->path, strlen(h->path), h);
}

/* retrieve an entry from the attribute cache */
attr_t *attr_cache_find(const char *path)
{
  attr_t *h;
  HASH_FIND_STR(attr_hash, path, h);
  if (h) {
    DEBUG("ATTR CACHE: found hash entry for %s\n", path);
    if (time(NULL) - h->timestamp > ATTR_CACHE_TIMEOUT && !h->modified) {
      DEBUG("ATTR CACHE: timeout for entry %s, deleting\n", path);
      HASH_DEL(attr_hash, h);
      free_attr(h);
      return NULL;
    }
  }
  return h;
}

/* free() memory used by attribute cache entries */
void attr_cache_free(void)
{
  attr_t *h, *tmp;
  /* delete every attr_hash entry in the table and in memory */
  HASH_ITER(hh, attr_hash, h, tmp) {
    HASH_DEL(attr_hash, h);
    free_attr(h);
  }
}

/* clear directory cache */
void dir_cache_init(void)
{
  dir_hash = NULL;
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
  free(d);
}

/* create a new directory cache entry */
dir_t *dir_cache_new(const char *path)
{
  dir_t *d;
  HASH_FIND_STR(dir_hash, path, d);
  /* we don't care about collisions, but we need to free() an old entry there is one */
  if (d) {
    DEBUG("DIR CACHE: found old entry for %s\n", path);
    HASH_DEL(dir_hash, d);
    free_dir(d);
  }

  d = calloc(1, sizeof(dir_t));
  d->path = strdup(path);
  d->entries = NULL;
  d->num_entries = 0;
  d->timestamp = time(NULL);
  
  DEBUG("DIR CACHE: adding new entry for %s\n", path);
  HASH_ADD_KEYPTR(hh, dir_hash, d->path, strlen(d->path), d);
  
  return d;
}

/* add a node to a directory cache entry */
void dir_cache_add(dir_t *dir, const char *name, int is_dir)
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
dir_t *dir_cache_find(const char *path)
{
  dir_t *d;
  HASH_FIND_STR(dir_hash, path, d);
  if (!d) {
    DEBUG("DIR CACHE: no entry found for %s\n", path);
    return NULL;
  }
  else {
    DEBUG("DIR CACHE: found entry for %s\n", path);
    if (time(NULL) - d->timestamp > DIR_CACHE_TIMEOUT && !d->modified) {
      DEBUG("DIR CACHE: timeout for entry %s, deleting\n", path);
      HASH_DEL(dir_hash, d);
      free_dir(d);
      return NULL;
    }
    return d;
  }
}

/* free() memory used by directory cache entries */
void dir_cache_free(void)
{
  dir_t *d, *tmp;
  HASH_ITER(hh, dir_hash, d, tmp) {
    HASH_DEL(dir_hash, d);
    free_dir(d);
  }
}
