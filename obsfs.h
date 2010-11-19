/* cache timeouts */

/* Directories are updated frequently as packages build or fail, but
   attributes rarely ever change.  In fact, most of them are entirely made
   up to begin with...  It therefore makes sense to have a much larger
   timeout for the attribute cache, which reduces server load. */

#define DIR_CACHE_TIMEOUT 20
#define ATTR_CACHE_TIMEOUT 3600
