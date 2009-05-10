#!/usr/bin/env python
import sys
import os
from CrossEnvironment import CrossEnvironment
from API import *

Import('ac_vars', 'srcdir', 'builddir')

# Ensures that we have all the elements required
feature_default = Feature(
        headers = [],
        bugle_libs = [],
        bugle_sources = [],
        bugleutils_sources = [])
feature_gl = Feature(
        gltype = 'gl',
        headers = ['GL/gl.h', 'GL/glext.h'])
feature_gles1 = Feature(
        gltype = 'gles1cm',
        headers = ['GLES/gl.h', 'GLES/glext.h'])
feature_gles2 = Feature(
        gltype = 'gles2',
        headers = ['GLES2/gl2.h', 'GLES2/gl2ext.h'])

feature_glx = Feature(
        glwin = 'glx',
        headers = ['GL/glx.h', 'GL/glxext.h'],
        bugle_sources = [srcdir.File('src/glx/glxdump.c')])
feature_wgl = Feature(
        glwin = 'wgl',
        headers = ['wingdi.h', 'GL/wglext.h'])
feature_egl = Feature(
        glwin = 'egl',
        headers = ['EGL/egl.h', 'EGL/eglext.h'])

feature_x11 = Feature(
        winsys = 'x11',
        bugle_libs = ['X11'])

apis = [API('gl-glx', feature_gl, feature_glx, feature_x11),
        API('gl-wgl', feature_gl, feature_wgl),
        API('gles1cm-egl-posix', feature_gles1, feature_egl),
        API('gles1cm-legacy-egl-win', feature_gles1, feature_egl),
        API('gles2-egl-posix', feature_gles2, feature_egl, feature_x11),
        API('gles2-egl-win', feature_gles2, feature_egl)]

# Process command line arguments
api = None
for i in apis:
    if i.name == ARGUMENTS.get('api', ac_vars['BUGLE_API']):
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
if 'gcc' in env['TOOLS']:
    env.MergeFlags('-fvisibility=hidden')

Export('build_env', 'env', 'api')

SConscript('lib/SConscript')
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
