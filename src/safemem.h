/* The function names and functionality are modelled on libiberty,
 * but are independently implemented.
 */

#ifndef BUGLE_SRC_SAFEMEM_H
#define BUGLE_SRC_SAFEMEM_H

#if HAVE_CONFIG_H
# include <stddef.h>
#endif

void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

#endif /* !BUGLE_SRC_SAFEMEM_H */
