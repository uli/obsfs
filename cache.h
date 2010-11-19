#include <sys/stat.h>
#include "uthash.h"

typedef struct {
  char *path;
  struct stat st;
  char *symlink;
  char *hardlink;
  time_t timestamp;
  int modified;
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
  UT_hash_handle hh;
} dir_t;

/* attribute cache methods */
void attr_cache_init(void);
void attr_cache_add(const char *path, struct stat *st, const char *symlink, const char *hardlink);
attr_t *attr_cache_find(const char *path);
void attr_cache_free(void);

/* directory cache methods */
void dir_cache_init(void);
dir_t *dir_cache_new(const char *path);
void dir_cache_add(dir_t *dir, const char *name, int is_dir);
dir_t *dir_cache_find(const char *path);
void dir_cache_free(void);
