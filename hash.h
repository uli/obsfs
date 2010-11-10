#include <sys/stat.h>

typedef struct {
  char *name;
  int is_dir;
} dirent_t;

typedef struct {
  char *path;
  dirent_t *entries;
  int num_entries;
} dir_t;

void hash_init(void);
void hash_add_attr(const char *path, struct stat *st);
struct stat *hash_find_attr(const char *path);
void hash_free(void);

void dir_cache_init(void);
dir_t *dir_new(const char *path);
void dir_add(dir_t *dir, const char *name, int is_dir);
int dir_find(dirent_t **dir, const char *path);
void dir_cache_free(void);
