#!/usr/bin/awk -f
# There are two ways in which GLenum can be implemented: as a C enum or as
# an integer type with #defined values. These rules attempt to handle both
# cases. There are also certain things that look like enums but are not.
# These are the GL_VERSION_1_1 etc strings, and bitfields like
# GL_ALL_CLIENT_ATTRIB_BITS. To make things more confusing, both real
# enums and bitfields can end in _BITS. We also need to consider extension
# suffices.
function is_enum(name)
{
    return !(name ~ /^GL_VERSION_[0-9_]+$/) \
           && !(name ~ /^GL_[A-Z0-9_]+_BIT(_[A-Z0-9]+)?$/) \
           && !(name ~ /^GL(_[A-Z0-9_])?_ALL_[A-Z0-9_]+_BITS(_[A-Z0-9]+)?$/);
}
/^#define GL_[0-9A-Z_]+[ \t]+(0x)?[0-9A-Fa-f]+/ && is_enum($2) \
{
    printf "%s %s\n", $2, $3
}
/^[ \t]+GL_[0-9A-Z_]+[ \t]+=[ \t]+(0x)?[0-9A-Fa-f]+/ && is_enum($1) \
{
    split($3, A, ",")
    printf "%s %s\n", $1, A[1]
}
