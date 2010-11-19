#include <sys/types.h>
#include <regex.h>

int mkdirp(const char *pathname, mode_t mode);
char *dirname_c(const char *path, char **basenm);
char *make_url(const char *url_prefix, const char *path);

char *get_match(regmatch_t match, const char *str);
