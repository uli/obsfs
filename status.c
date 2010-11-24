/*
 * status.c
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

#include "status.h"
#include <stdlib.h>
#include <expat.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

struct status_s {
  int ret;
  XML_Parser xp;
};

struct scode_s {
  const char *code;
  int err;
} statuses[] = {
 {"access_no_permission", EPERM},
 {"binary_download_no_permission", EPERM},
 {"change_attribute_no_permission", EPERM},
 {"change_package_protection_level", EPERM},
 {"change_project_no_permission", EPERM},
 {"change_project_protection_level", EPERM},
 {"cmd_execution_no_permission", EPERM},
 {"create_project_no_permission", EPERM},
 {"delete_file_no_permission", EPERM},
 {"delete_project_no_permission", EPERM},
 {"delete_project_pubkey_no_permission", EPERM},
 {"download_binary_no_permission", EPERM},
 {"double_branch_package", EEXIST},
 {"illegal_request", EINVAL},
 {"invalid_filelist", EINVAL},
 {"invalid_flag", EINVAL},
 {"invalid_package_name", EINVAL},
 {"invalid_project_name", EINVAL},
 {"invalid_xml", EINVAL},
 {"internal_error", EBADF},
 {"modify_project_no_permission", EPERM},
 {"no_matched_binaries", ENOENT},
 {"not_found", ENOENT},
 {"project_name_mismatch", EINVAL},
 {"put_file_no_permission", EPERM},
 {"put_project_config_no_permission", EPERM},
 {"save_error", EIO},
 {"source_access_no_permission", EPERM},
 {"spec_file_exists", EEXIST},
 {"unknown_operation", EINVAL},
 {"unknown_package", ENOENT},
 {"unknown_project", ENOENT},
 {"unknown_repository", ENOENT},
 {NULL, 0}
};
  
static void xml_status_tag_start(void *ud, const XML_Char *name, const XML_Char **atts)
{
  status_t *status = (status_t *)ud;
  if (!strcmp(name, "status")) {
    for (; *atts; atts += 2) {
      if (!strcmp(atts[0], "code")) {
        struct scode_s *sc;
        for (sc = statuses; sc->code; sc++) {
          if (!strcmp(atts[1], sc->code)) {
            status->ret = sc->err;
          }
        }
      }
    }
  }
}

static void xml_status_tag_end(void *ud, const XML_Char *name)
{
}

status_t *xml_status_init(void)
{
  status_t *status = malloc(sizeof(status_t));
  status->ret = 0;
  
  status->xp = XML_ParserCreate(NULL);
  if (!status->xp)
    abort();
  XML_SetUserData(status->xp, (void *)status);
  XML_SetElementHandler(status->xp, xml_status_tag_start, xml_status_tag_end);
  
  return (void *)status;
}

int xml_status_write(void *ptr, size_t size, size_t nmemb, void *vst)
{
  status_t *status = (status_t *)vst;
  fwrite(ptr, size, nmemb, stderr);
  XML_Parse(status->xp, ptr, size * nmemb, 0);
  return size * nmemb;
}

int xml_get_status(status_t *status)
{
  return status->ret;
}

void xml_status_destroy(status_t *status)
{
  XML_ParserFree(status->xp);
  free(status);
}
