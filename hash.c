#include "obsfs.h"
#include "hash.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
  char *path;
  struct stat st;
} hash_t;

hash_t attr_hash[ATTR_CACHE_SIZE];

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

void hash_add(const char *path, struct stat *st)
{
  hash_t *h = &attr_hash[hash_string(path) % ATTR_CACHE_SIZE];
  if (h->path)
    free(h->path);
  h->path = strdup(path);
  h->st = *st;
}

struct stat *hash_find(const char *path)
{
  hash_t *h = &attr_hash[hash_string(path) % ATTR_CACHE_SIZE];
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
