/*
 * util.h
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

#include <sys/types.h>
#include <regex.h>

int mkdirp(const char *pathname, mode_t mode);
char *dirname_c(const char *path, char **basenm);
char *make_url(const char *url_prefix, const char *path, const char *rev);

char *get_match(regmatch_t match, const char *str);

int is_a_file(const char *path, const char *filename);
int endswith(const char *str, const char *end);
