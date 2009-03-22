#ifndef BUGLE_GLX_OVERRIDES_H
#define BUGLE_GLX_OVERRIDES_H

/* X #defines this rather than typedefing it. Unfortunately this means that
 * we have to give it a new type and explicitly tag uses.
 */
typedef Bool XBool;

/* Fake type to support dumping by name */
typedef int GLXenum;

#endif /* BUGLE_GLX_OVERRIDES_H */
