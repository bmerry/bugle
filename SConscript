#!/usr/bin/env python
import sys
import os
from CrossEnvironment import CrossEnvironment
from API import *
import BugleChecks

Import('ac_vars', 'srcdir', 'builddir')

def check_gl(conf, lib, headers):
    '''
    Looks for an OpenGL library of the appropriate name.

    @param conf The configuration context to use for the check
    @type conf  SCons.Configure
    @param lib  The library to look for. If the name starts of "lib", it
                will be removed on platforms where that is already a library
                prefix.
    @type lib   strings
    @param headers The header files to include for the check. The last item
                   must be the actual header file with GL defines
    @type headers list of strings
    '''

    if lib.startswith('lib') and conf.env.subst('${SHLIBPREFIX}') == 'lib':
        lib = lib[3:]
    if not conf.CheckCHeader(headers, '<>'):
        print '<' + headers[-1] + '> not found'
        Exit(1)
    if not conf.CheckLibWithHeader(lib, headers, 'c', 'glFlush();', False):
        print conf.env.subst('${SHLIBPREFIX}' + lib + '${SHLIBSUFFIX} not found')
        Exit(1)

def get_features():
    '''
    Returns a dictionary of Feature objects
    '''

    return {
            'gl': Feature(
                gltype = 'gl',
                headers = ['GL/gl.h', 'GL/glext.h']),
            'gles1': Feature(
                gltype = 'gles1cm',
                headers = ['GLES/gl.h', 'GLES/glext.h']),
            'gles2': Feature(
                gltype = 'gles2',
                checks = [lambda conf: check_gl(conf, 'GLESv2', ['GLES2/gl2.h'])],
                headers = ['GLES2/gl2.h', 'GLES2/gl2ext.h']),

            'glx': Feature(
                glwin = 'glx',
                headers = ['GL/glx.h', 'GL/glxext.h'],
                bugle_sources = ['#src/glx/glxdump.c']),
            'wgl': Feature(
                glwin = 'wgl',
                headers = ['wingdi.h', 'GL/wglext.h']),
            'egl': Feature(
                glwin = 'egl',
                headers = ['EGL/egl.h', 'EGL/eglext.h']),

            'x11': Feature(
                winsys = 'x11',
                bugle_libs = ['X11']),

            'gl-wgl': Feature(
                checks = [lambda conf: check_gl(conf, 'opengl32', ['windows.h', 'GL/gl.h'])]
                ),
            'gl-glx': Feature(
                checks = [lambda conf: check_gl(conf, 'GL', ['GL/gl.h'])]
                ),
            'gles1-legacy': Feature(
                checks = [lambda conf: check_gl(conf, 'GLES_CM', ['GLES/gl.h'])]
                ),
            'gles1-new': Feature(
                checks = [lambda conf: check_gl(conf, 'GLESv1_CM', ['GLES/gl.h'])]
                )
            }

def get_apis():
    '''
    Returns a list of possible API objects
    '''

    # Each list is a name followed by a list of feature keys
    api_helpers = [
            ['gl-glx', 'gl', 'glx', 'gl-glx', 'x11'],
            ['gl-wgl', 'gl', 'wgl', 'gl-wgl'],
            ['gles1cm-egl-posix', 'gles1', 'gles1-new', 'egl'],
            ['gles1cm-legacy-egl-win', 'gles1', 'gles1-legacy', 'egl'],
            ['gles2-egl-posix', 'gles2', 'egl', 'x11'],
            ['gles2-egl-win', 'gles2', 'egl']
            ]
    apis = []
    features = get_features()
    for i in api_helpers:
        f = [features[x] for x in i[1:]]
        apis.append(API(i[0], *f))
    return apis

def get_api(name):
    '''
    Get the API object for the selected variant string
    '''
    apis = get_apis()
    for i in apis:
        if i.name == name:
            return i
    print >>sys.stderr, "Invalid value '%s' for api" % name
    Exit(2)

# Process command line arguments
api = get_api(ARGUMENTS.get('api', ac_vars['BUGLE_API']))

environ = {}
for e in ['PATH', 'CPATH', 'LIBRARY_PATH', 'LD_LIBRARY_PATH']:
    if e in os.environ:
        environ[e] = os.environ[e]

# Values common to both build environment and host environment
common_kw = dict(
        ENV = environ,
        CPPPATH = [
            srcdir,
            srcdir.Dir('lib'),
            srcdir.Dir('include'),
            builddir,
            builddir.Dir('include')
            ],
        toolpath = ['%(srcdir)s/site_scons/site_tools' % ac_vars],
        srcdir = srcdir,
        builddir = builddir)

# Environment for tools that have to run on the build machine
build_env = Environment(
        CPPDEFINES = [
            ('HAVE_CONFIG_H', 1)
            ],
        YACCHXXFILESUFFIX = '.h',
        tools = ['default', 'budgie', 'gengl', 'yacc'],

        BCPATH = [builddir, srcdir],
        **common_kw)

# Environment for the target machine
env = CrossEnvironment(
        CPPDEFINES = [
            ('LIBDIR', r'\"%(libdir)s\"' % ac_vars),
            ('PKGLIBDIR', r'\"%(libdir)s/%(PACKAGE)s\"' % ac_vars),
            ('HAVE_CONFIG_H', 1)
            ],
        parse_flags = '-pthread',
        tools = ['default', 'yacc'],
        **common_kw
        )
if 'gcc' in env['TOOLS']:
    env.MergeFlags('-fvisibility=hidden')

conf = Configure(env, custom_tests = BugleChecks.tests, config_h = '#/config.h')
conf.CheckHeader('dirent.h')
conf.CheckHeader('ndir.h')
conf.CheckHeader('dlfcn.h')
conf.CheckHeader('stdint.h')
conf.CheckLibWithHeader('m', 'math.h', 'c', 'finite(1.0);', autoadd = False)
conf.CheckLibWithHeader('m', 'math.h', 'c', 'isfinite(1.0);', autoadd = False)
conf.CheckLibWithHeader('m', 'math.h', 'c', 'sinf(1.0f);', autoadd = False)
conf.CheckFunc('strtof')
conf.CheckAttributePrintf()
conf.CheckAttributeConstructor()
conf.CheckAttributeHiddenAlias()
for c in api.checks:
    c(conf)
env = conf.Finish()

Export('build_env', 'env', 'api')

SConscript('lib/SConscript')
SConscript('budgie/SConscript')
SConscript('src/SConscript')
SConscript('filters/SConscript')

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
