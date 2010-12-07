/*
 * obsfs.h
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

#define OBSFS_VERSION "0.1"

/* cache timeouts */

/* Directories are updated frequently as packages build or fail, but
   attributes rarely ever change.  In fact, most of them are entirely made
   up to begin with...  It therefore makes sense to have a much larger
   timeout for the attribute cache, which reduces server load. */

#define DIR_CACHE_TIMEOUT 20
#define ATTR_CACHE_TIMEOUT 3600
#define FILE_CACHE_TIMEOUT 600

#define DEFAULT_HOST "api.opensuse.org"

#define NODE_FAILED "_failed"
