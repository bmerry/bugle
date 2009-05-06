#!/usr/bin/env python
import sys
import os
from CrossEnvironment import CrossEnvironment

Import('ac_vars', 'srcdir', 'builddir')

# Process command line arguments
api = ARGUMENTS.get('api', 'gl-glx')
if api == 'gl-glx':
    pass
elif api == 'gl-wgl':
    pass
elif api == 'gles1cm-egl-posix':
    pass
elif api == 'gles2-egl-posix':
    pass
else:
    print >>sys.stderr, "Invalid value for api"
    Exit(2)

# Environment for tools that have to run on the build machine
build_env = Environment(
        ENV = {'PATH': os.environ['PATH']},
        CPPPATH = [
            srcdir,
            srcdir.Dir('lib'),
            srcdir.Dir('include'),
            builddir,
            builddir.Dir('include')
            ],
        CPPDEFINES = [
            ('HAVE_CONFIG_H', 1)
            ],
        YACCHXXFILESUFFIX = '.h',
        tools = ['default', 'budgie', 'gengl'],
        toolpath = ['%(srcdir)s/site_scons/site_tools' % ac_vars],

        BCPATH = [builddir, srcdir],
        srcdir = srcdir, builddir = builddir)

# Environment for the target machine
env = CrossEnvironment(
        ENV = {'PATH': os.environ['PATH']},
        HOST = 'arm-none-linux-gnueabi',
        CPPPATH = [
            srcdir,
            srcdir.Dir('lib'),
            srcdir.Dir('include'),
            builddir,
            builddir.Dir('include')
            ],
        CPPDEFINES = [
            ('LIBDIR', r'\"%(libdir)s\"' % ac_vars),
            ('PKGLIBDIR', r'\"%(libdir)s/%(PACKAGE)s\"' % ac_vars),
            ('HAVE_CONFIG_H', 1)
            ],
        parse_flags = '-pthread',

        srcdir = srcdir, builddir = builddir
        )
Export('build_env', 'env')

SConscript('budgie/SConscript')
SConscript('src/SConscript')
SConscript('filters/SConscript')

env.AlwaysBuild(env.Command('lib/libgnu.la', [], 'make -C lib'))

budgie_outputs = [
        'include/budgie/call.h',
        'include/budgie/types2.h',
        'budgielib/defines.h',
        'budgielib/tables.c',
        'budgielib/lib.c'
        ]
build_env.Budgie(budgie_outputs, ['src/data/gl.tu', 'bc/gl-glx.bc'])

headers = ['GL/gl.h', 'GL/glext.h', 'GL/glx.h', 'GL/glxext.h']
headers = [build_env['find_header'](env, h) for h in headers]
# TODO change to bc/alias.bc
build_env.BudgieAlias(target = ['bc/gl/alias.bc'], source = headers)
build_env.ApitablesC(target = ['src/apitables.c'], source = ['budgielib/defines.h'] + headers)
build_env.ApitablesH(target = ['src/apitables.h'], source = ['budgielib/defines.h'] + headers)
