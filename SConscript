#!/usr/bin/env python
import sys
import os
from CrossEnvironment import CrossEnvironment

Import('ac_vars', 'srcdir', 'builddir')

class API:
    def __init__(self, name, headers):
        self.name = name
        self.headers = headers

        self.fields = name.split('-')
        for gltype in ['gl', 'gles1cm', 'gles2']:
            if gltype in self.fields:
                self.gltype = gltype
        for glwin in ['glx', 'wgl', 'egl']:
            if glwin in self.fields:
                self.glwin = glwin

gl_headers = ['GL/gl.h', 'GL/glext.h']
gles1_headers = ['GLES/gl.h', 'GLES/glext.h']
gles2_headers = ['GLES2/gl2.h', 'GLES2/gl2ext.h']
glx_headers = ['GL/glx.h', 'GL/glxext.h']
wgl_headers = ['wingdi.h', 'GL/wglext.h']
egl_headers = ['EGL/egl.h', 'EGL/eglext.h']
apis = [API('gl-glx', gl_headers + glx_headers),
        API('gl-wgl', gl_headers + wgl_headers),
        API('gles1cm-egl-posix', gles1_headers + egl_headers),
        API('gles1cm-legacy-egl-win', gles1_headers + egl_headers),
        API('gles2-egl-posix', gles2_headers + egl_headers),
        API('gles2-egl-win', gles2_headers + egl_headers)]

# Process command line arguments
api = None
for i in apis:
    if i.name == ARGUMENTS.get('api', 'gl-glx'):
        api = i
        break

if api is None:
    print >>sys.stderr, "Invalid value for api"
    Exit(2)

# Environment for tools that have to run on the build machine
environ = {}
for e in ['PATH', 'CPATH', 'LIBRARY_PATH', 'LD_LIBRARY_PATH']:
    if e in os.environ:
        environ[e] = os.environ[e]
build_env = Environment(
        ENV = environ,
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
        tools = ['default', 'budgie', 'gengl', 'yacc'],
        toolpath = ['%(srcdir)s/site_scons/site_tools' % ac_vars],

        BCPATH = [builddir, srcdir],
        srcdir = srcdir, builddir = builddir)

# Environment for the target machine
env = CrossEnvironment(
        ENV = environ,
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
        tools = ['default', 'yacc'],

        srcdir = srcdir, builddir = builddir
        )
Export('build_env', 'env', 'api')

SConscript('budgie/SConscript')
SConscript('src/SConscript')
SConscript('filters/SConscript')

env.AlwaysBuild(env.Command('lib/libgnu.la', [], 'make -C lib'))
env.AlwaysBuild(env.Command('libltdl/libltdl.la', [], 'make -C libltdl'))

budgie_outputs = [
        'include/budgie/call.h',
        'include/budgie/types2.h',
        'budgielib/defines.h',
        'budgielib/tables.c',
        'budgielib/lib.c'
        ]
build_env.Budgie(budgie_outputs, ['src/data/gl.tu', 'bc/' + api.name + '.bc'])

headers = [build_env['find_header'](env, h) for h in api.headers]
# TODO change to bc/alias.bc
build_env.BudgieAlias(target = ['bc/gl/alias.bc'], source = headers)
build_env.ApitablesC(target = ['src/apitables.c'], source = ['budgielib/defines.h'] + headers)
build_env.ApitablesH(target = ['src/apitables.h'], source = ['budgielib/defines.h'] + headers)
