#!/usr/bin/env python
import sys
import os
from SCons.Script.Main import OptionsParser
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
    Returns a dictionary of possible API objects
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
    apis = {}
    features = get_features()
    for i in api_helpers:
        f = [features[x] for x in i[1:]]
        apis[i[0]] = API(i[0], *f)
    return apis

def get_default_api_name(env):
    if env['PLATFORM'] == 'posix':
        return 'gl-glx'
    else:
        return 'gl-wgl'

def setup_options():
    '''
    Adds custom command-line options
    '''
    AddOption('--short', help = 'Show more compact build lines', dest = 'bugle_short', action = 'store_true')

def checks(env):
    '''
    Do configuration checks
    '''
    if env.GetOption('help') or env.GetOption('clean'):
        return env

    conf = Configure(env, custom_tests = BugleChecks.tests, config_h = '#/config.h')
    conf.CheckLib('m')
    conf.CheckHeader('dlfcn.h')
    conf.CheckHeader('stdint.h')
    for i in ['cosf', 'sinf', 'finite', 'isfinite', 'isnan', 'nan', 'strtof']:
        conf.CheckFunc(i)
    conf.CheckDeclaration('va_copy', '#include <stdarg.h>')
    conf.CheckDeclaration('__va_copy', '#include <stdarg.h>')
    conf.CheckAttributePrintf()
    conf.CheckAttributeConstructor()
    conf.CheckAttributeHiddenAlias()
    conf.CheckInline()
    for c in api.checks:
        c(conf)
    # TODO actually do a test here
    conf.Define('USE_POSIX_THREADS', 1)
    return conf.Finish()

def get_vars(apis):
    vars = Variables('config.py', ARGUMENTS)
    vars.AddVariables(
            EnumVariable('api', 'GL API variant', get_default_api_name(DefaultEnvironment()), allowed_values = apis.keys()),
            ('HOST', 'Host machine string for cross-compilation'),
            ('CC', 'The C compiler'),
            ('CCFLAGS', 'C and C++ compilation flags'),
            ('CFLAGS', 'C compilation flags'),
            ('CXXFLAGS', 'C++ compilation flags'))
    return vars

def get_substs(platform, api):
    '''
    Obtains a dictionary of substitutions to make in porting.h
    '''

    substs = {}
    if platform == 'posix':
        substs['BUGLE_OSAPI'] = 'BUGLE_OSAPI_POSIX'
        substs['BUGLE_FS'] = 'BUGLE_FS_UNIX'
        substs['BUGLE_BINFMT'] = 'BUGLE_BINFMT_ELF'
    elif platform == 'win32':
        substs['BUGLE_OSAPI'] = 'BUGLE_OSAPI_WIN32'
        substs['BUGLE_FS'] = 'BUGLE_FS_CYGMING'
        substs['BUGLE_BINFMT'] = 'BUGLE_BINFMT_PE'
    elif platform == 'cygwin':
        substs['BUGLE_OSAPI'] = 'BUGLE_OSAPI_POSIX'
        substs['BUGLE_FS'] = 'BUGLE_FS_CYGMING'
        substs['BUGLE_BINFMT'] = 'BUGLE_BINFMT_PE'
    else:
        print 'Platform', platform, 'is not recognised.'
        Exit(1)

    substs['BUGLE_GLTYPE'] = 'BUGLE_GLTYPE_' + api.gltype.upper()
    substs['BUGLE_GLWIN'] = 'BUGLE_GLWIN_' + api.glwin.upper()
    substs['BUGLE_WINSYS'] = 'BUGLE_WINSYS_' + api.winsys.upper()
    return substs

# Process command line arguments
apis = get_apis()
vars = get_vars(apis)

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
        builddir = builddir,
        variables = vars)

setup_options()
if GetOption('bugle_short'):
    for i in ['ALIAS', 'APITABLESC', 'APITABLESH', 'CC', 'CXX', 'LDMODULE', 'LEX', 'LINK', 'SHCC', 'SHCXX', 'SHLINK', 'YACC']:
        common_kw[i + 'COMSTR'] = i + ' $TARGET'
    common_kw['TUCOMSTR'] = 'TU ${TARGETS[1]}'
    common_kw['BUDGIECOMSTR'] = 'BUDGIE ${TARGETS}'

# Environment for tools that have to run on the build machine
build_env = Environment(
        YACCHXXFILESUFFIX = '.h',
        tools = ['default', 'yacc'],
        **common_kw)

# Environment for the target machine
env = CrossEnvironment(
        CPPDEFINES = [
            ('LIBDIR', r'\"%(libdir)s\"' % ac_vars),
            ('PKGLIBDIR', r'\"%(libdir)s/%(PACKAGE)s\"' % ac_vars),
            ('HAVE_CONFIG_H', 1)
            ],
        tools = ['default', 'yacc', 'subst'],
        **common_kw
        )
if 'gcc' in env['TOOLS']:
    env.MergeFlags('-fvisibility=hidden -Wstrict-prototypes -Wall -g')

# Environment for generating parse tree. This has to be GCC, and in cross
# compilation should be the cross-gcc so that the correct headers are
# identified for the target.
tu_env = CrossEnvironment(
        tools = ['gcc', 'budgie', 'gengl', 'tu'],
        BCPATH = [builddir, srcdir],
        **common_kw)

# Post-process command-line options
Help(vars.GenerateHelpText(env))
unknown = vars.UnknownVariables()
if unknown:
    print 'Unknown command-line option(s):', ', '.join(unknown.keys())
    Exit(2)
api = apis[env['api']]
substs = get_substs(env['PLATFORM'], api)
env.SubstFile(builddir.File('include/bugle/porting.h'), srcdir.File('include/bugle/porting.h.in'), substs)

env = checks(env)
Export('build_env', 'tu_env', 'env', 'api')

# Platform SConscript must be first, since it modifies the environment
# via config.h
SConscript('platform/posix/SConscript')

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
tu_env.Budgie(budgie_outputs, ['src/data/gl.tu', 'bc/' + api.name + '.bc'])

headers = [tu_env['find_header'](env, h) for h in api.headers]
# TODO change to bc/alias.bc
tu_env.BudgieAlias(target = ['bc/alias.bc'], source = headers)
tu_env.ApitablesC(target = ['src/apitables.c'], source = ['budgielib/defines.h'] + headers)
tu_env.ApitablesH(target = ['src/apitables.h'], source = ['budgielib/defines.h'] + headers)
