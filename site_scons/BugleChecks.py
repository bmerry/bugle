#!/usr/bin/env python

def check_attribute(ctx, name, define, prototype):
    '''
    Checks whether a GCC attribute is supported

    @param ctx A CheckContext
    @param name A human-readable description of the attribute
    @param define The variable to define in config.h if support is present
    @param prototype The function prototype to test, including semi-colon
    '''

    ctx.Message('Checking for GCC %s attribute...' % name)
    ret = ctx.TryCompile('''
%s

int main()
{
    return 0;
}
''' % prototype, '.c')
    ctx.Result(ret)
    if ret:
        ctx.sconf.Define(define, 1, 'Define to 1 if GCC %s __attribute__ is supported' % name)
    return ret

def check_attribute_printf(ctx):
    return check_attribute(ctx, 'printf', 'BUGLE_HAVE_ATTRIBUTE_FORMAT_PRINTF',
            'void bugle_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));')

def check_attribute_constructor(ctx):
    ret = check_attribute(ctx, 'constructor', 'BUGLE_HAVE_ATTRIBUTE_CONSTRUCTOR',
            'void bugle_constructor(void) __attribute__((constructor));')
    if ret:
        value = '__attribute__((constructor))'
    else:
        value = ''
    ctx.sconf.Define('BUGLE_ATTRIBUTE_CONSTRUCTOR_ATTRIBUTE', value)
    return ret

def check_attribute_hidden_alias(ctx):
    return check_attribute(ctx, 'hidden alias', 'BUGLE_HAVE_ATTRIBUTE_HIDDEN_ALIAS',
            '''
void __attribute__((visibility("default"))) bugle_public(void) {}
extern __typeof(bugle_public) bugle_alias __attribute__((alias("bugle_public"), visibility("hidden")));
#if defined(_WIN32) || defined(__CYGWIN__)
# error "Cygwin accepts but ignores hidden visibility"
#endif''')

def _has_inline(ctx, subst):
    ctx.Message('Checking for ' + subst + '...')
    ret = ctx.TryCompile('''
static %s int my_inline_func(void)
{
    return 0;
}

int main() { return my_inline_func(); }
''' % subst, '.c')
    ctx.Result(ret)
    return ret

def check_inline(ctx):
    '''
    Defines inline suitably
    '''

    ret = True
    if _has_inline(ctx, 'inline'):
        pass
    elif _has_inline(ctx, '__inline__'):
        ctx.sconf.Define('inline', '__inline__')
    elif _has_inline(ctx, '__inline'):
        ctx.sconf.Define('inline', '__inline')
    else:
        ctx.sconf.Define('inline', '')
        ret = False
    return ret

tests = {
        'CheckAttributePrintf': check_attribute_printf,
        'CheckAttributeConstructor': check_attribute_constructor,
        'CheckAttributeHiddenAlias': check_attribute_hidden_alias,
        'CheckInline': check_inline
        }
