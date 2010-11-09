#include <sys/stat.h>

void hash_init(void);
void hash_add(const char *path, struct stat *st);
struct stat *hash_find(const char *path);
void hash_free(void);
