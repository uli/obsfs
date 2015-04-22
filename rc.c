/*
 * rc.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <glib.h>
#include <bzlib.h>

#include "rc.h"

//#define DEBUG_RC

#ifdef DEBUG_RC
#define DEBUG(x...) fprintf(stderr, x)
#else
#define DEBUG(x...)
#endif

/* Get the user name and password for a given server from a given .oscrc file, or from ~/.oscrc
   if none specified, and store pointers to them in *user and *pass. */
int rc_get_account(const char *server, const char *home, const char *oscrc_config, char **user, char **pass)
{
  char *oscrc;
  if (oscrc_config) {
    oscrc = strdup(oscrc_config);
  }
  else {
    oscrc = malloc(strlen(home) + strlen("/.oscrc") + 1);
    sprintf(oscrc, "%s%s", home, "/.oscrc");
  }
  
  FILE *fp = fopen(oscrc, "r");
  free(oscrc);
  if (!fp)
    return -1;
  
  /* find the config file section for the given host and extract login and password */
  char buf[255];
  char *password = NULL;
  char *username = NULL;

  regmatch_t matches[3];
  regex_t r;
  /* depending on the moon phase, hosts are either given as FQDN, or as HTTP or HTTPS URL */
  regcomp(&r, "^[[](https?://)?([^/]+)[]/]", REG_EXTENDED);
  /* regexes for login ("user=...") and obfuscated password ("passx=...") */
  regex_t pr, prx, ur;
  regcomp(&pr, "^[[:space:]]*pass[[:space:]]*=[[:space:]]*(.*)\n$", REG_EXTENDED);
  regcomp(&prx, "^[[:space:]]*passx[[:space:]]*=[[:space:]]*(.*)\n$", REG_EXTENDED);
  regcomp(&ur, "^[[:space:]]*user[[:space:]]*=[[:space:]]*(.*)\n$", REG_EXTENDED);

  while (fgets(buf, 255, fp)) {
next:
    if (!regexec(&r, buf, 3, matches, 0)) {
      /* is this is our host's section? */
      if (!strncmp(buf + matches[2].rm_so, server, matches[2].rm_eo - matches[2].rm_so)) {
        
        while (fgets(buf, 255, fp)) {
          if (buf[0] == '[') {
            /* next section starts here; can't just continue the outermost loop
               because we'd skip the current line, thus the goto... */
            goto next;
          }
          
          /* pass */
          if (!regexec(&pr, buf, 3, matches, 0)) {
            if (password) {
              continue;
            }
            buf[matches[1].rm_eo] = 0;
            password = strdup(buf + matches[1].rm_so);
            DEBUG("pass %s\n", username);
          }
          /* passx */
          else if (!regexec(&prx, buf, 3, matches, 0)) {
            if (password) {
              continue;
            }
            /* passx is base64-encoded bzip2-compressed plaintext. No comment... */
            gsize len;
            guchar *bz2pass = g_base64_decode(buf + matches[1].rm_so, &len);
            if (bz2pass) {
              unsigned int dec_len = 255;
              if (BZ2_bzBuffToBuffDecompress(buf, &dec_len, (char *)bz2pass, len, 0, 0) == BZ_OK) {
                buf[dec_len] = 0;	/* BZ2_bzBuffToBuffDecompress() does not write a terminating 0 */
                DEBUG("pass %d %s\n", dec_len, buf);
                password = strdup(buf);
              }
              free(bz2pass);
            }
          }
          /* user */
          else if (!regexec(&ur, buf, 3, matches, 0)) {
            buf[matches[1].rm_eo] = 0;
            username = strdup(buf + matches[1].rm_so);
            DEBUG("user %s\n", username);
          }
        }
      }
    }
  }

  regfree(&pr);
  regfree(&prx);
  regfree(&ur);
  regfree(&r);
  fclose(fp);
  
  if (password && username) {
    *pass = password;
    *user = username;
    return 0;
  }
  else {
    /* no complete login found */
    return -1;
  }
}
