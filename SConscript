#!/usr/bin/env python
import sys
import os
from SCons.Script.Main import OptionsParser
from CrossEnvironment import CrossEnvironment
from Aspects import *
import BugleChecks

Import('ac_vars', 'srcdir', 'builddir')

class Target(dict):
    '''
    Contains the keywords that will be passed to a builder. This allows
    the sources, libraries etc, to be built up from different places
    '''
    def __init__(self):
        self['source'] = []
        self['target'] = []
        self['LIBS'] = ['$LIBS']    # ensures we add to rather than replace

targets = {
        'bugle': Target(),
        'bugleutils': Target()
        }

def check_gl(conf, lib, headers):
    '''
    Looks for an OpenGL library of the appropriate name.

    @param conf The configuration context to use for the check
    @type conf  SCons.Configure
    @param lib  The library to look for. If the name starts of "lib", it
                will be removed on platforms where that is already a library
                prefix.
    @type lib   string
    @param headers The header files to include for the check.
    @type headers list of strings
    '''

    if lib.startswith('lib') and conf.env.subst('${SHLIBPREFIX}') == 'lib':
        lib = lib[3:]

    # Incrementally check the headers
    for i in range(len(headers)):
        if not conf.CheckCHeader(headers[:(i+1)], '<>'):
            print headers[i] + ' not found'
            Exit(1)
    if not conf.CheckLibWithHeader(lib, headers, 'c', 'glFlush();', False):
        print conf.env.subst('${SHLIBPREFIX}' + lib + '${SHLIBSUFFIX} not found')
        Exit(1)

def get_features():
    '''
    Returns a dictionary of Feature objects
    '''

    return {
            'gl': Feature(),
            'gles1': Feature(
                gltype = 'gles1cm'),
            'gles2': Feature(
                gltype = 'gles2'),

            'glx': Feature(
                glwin = 'glx'),
            'wgl': Feature(
                winsys = 'windows',
                glwin = 'wgl'),
            'egl': Feature(
                glwin = 'egl'),

            'x11': Feature(
                winsys = 'x11'),

            'gl-wgl': Feature(
                ),
            'gl-glx': Feature(
                ),
            'gles1-legacy': Feature(
                ),
            'gles1-new': Feature(
                )
            }

def platform_default(aspect):
    compiler = aspects['host_compiler']
    if compiler == 'msvc':
        return 'msvcrt'
    else:
        return 'posix'

def winsys_default(aspect):
    platform = aspects['platform']
    if platform == 'msvcrt':
        return 'windows'
    else:
        return 'x11'

def glwin_default(aspect):
    gltype = aspects['gltype']
    winsys = aspects['winsys']
    if gltype == 'gl':
        if winsys == 'x11':
            return 'glx'
        elif winsys == 'windows':
            return 'wgl'
        else:
            return 'unknown'
    else:
        return 'egl'

def fs_default(aspect):
    platform = aspects['platform']
    if platform == 'msvcrt':
        return 'cygming'    # TODO add windows fs
    else:
        return 'unix'

def binfmt_default(aspect):
    platform = aspects['platform']
    if platform == 'msvcrt':
        return 'pe'
    else:
        return 'elf'

def apply_compiler(env, compiler, config):
    if compiler == 'msvc':
        env.Tool('msvc')
        env.Tool('mslink')
        env.Tool('mslib')
        if config == 'debug':
            env.Append(CCFLAGS = ['/Zi'])
            env.Append(LINKFLAGS = ['/DEBUG'])
        elif config == 'release':
            pass
        else:
            raise RuntimeError, 'Unknown config "%s"' % config

    elif compiler == 'gcc':
        env.Tool('gcc')
        env.Tool('g++')
        env.Tool('gnulink')
        env.Tool('ar')
        env.AppendUnique(CCFLAGS = ['-fvisibility=hidden', '-Wall'])
        env.AppendUnique(CFLAGS = ['-std=c89', '-Wstrict-prototypes'])
        if config == 'debug':
            env.MergeFlags('-g')
        elif config == 'release':
            env.MergeFlags('-O2 -s')
        else:
            raise RuntimeError, 'Unknown config "%s"' % config

    else:
        raise RuntimeError, 'Don\'t know how to apply compiler "%s"' % compiler

def setup_options():
    '''
    Adds custom command-line options
    '''
    AddOption('--short', help = 'Show more compact build lines', dest = 'bugle_short', action = 'store_true')

def setup_aspects():
    '''
    Creates an AspectParser object with the user-configurable options
    '''
    aspects = AspectParser()
    aspects.AddAspect(Aspect(
        name = 'build_compiler',
        help = 'Compiler for tools used to build the rest of bugle',
        choices = ['gcc', 'msvc'],
        default = 'gcc'))
    aspects.AddAspect(Aspect(
        name = 'host_compiler',
        help = 'Compiler for the final outputs',
        choices = ['gcc', 'msvc'],
        default = 'gcc'))
    aspects.AddAspect(Aspect(
        name = 'host',
        help = 'name for the host machine for cross-compiling',
        default = ''))
    aspects.AddAspect(Aspect(
        name = 'gltype',
        help = 'Variant of GL',
        choices = ['gl', 'gles1cm', 'gles2'],
        default = 'gl'))
    aspects.AddAspect(Aspect(
        name = 'glwin',
        help = 'Window system abstraction',
        choices = ['wgl', 'glx', 'egl'],
        default = glwin_default))
    aspects.AddAspect(Aspect(
        name = 'winsys',
        help = 'Windowing system',
        choices = ['windows', 'x11', 'none'],
        default = winsys_default))
    aspects.AddAspect(Aspect(
        name = 'fs',
        help = 'Location of files',
        choices = ['cygming', 'unix'],
        default = fs_default))
    aspects.AddAspect(Aspect(
        name = 'platform',
        help = 'C runtime libraries',
        choices = ['posix', 'msvcrt', 'null'],
        default = platform_default))
    aspects.AddAspect(Aspect(
        name = 'binfmt',
        help = 'Dynamic linker style',
        choices = ['pe', 'elf'],
        default = binfmt_default))
    aspects.AddAspect(Aspect(
        name = 'config',
        help = 'Compiler option set',
        choices = ['debug', 'release'],
        default = 'debug'))
    return aspects

def checks(env, gl_lib, gl_headers):
    '''
    Do configuration checks
    '''
    if env.GetOption('help') or env.GetOption('clean'):
        return env

    conf = Configure(env, custom_tests = BugleChecks.tests, config_h = '#/config.h')
    conf.CheckLib('m')
    conf.CheckHeader('dlfcn.h')
    conf.CheckHeader('stdint.h')
    for i in ['cosf', 'sinf', 'siglongjmp']:
        conf.CheckFunc(i)
    conf.CheckDeclaration('va_copy', '#include <stdarg.h>')
    conf.CheckDeclaration('__va_copy', '#include <stdarg.h>')
    conf.CheckAttributePrintf()
    conf.CheckAttributeConstructor()
    conf.CheckAttributeHiddenAlias()
    conf.CheckInline()
    check_gl(conf, gl_lib, gl_headers)
    return conf.Finish()

def get_vars():
    vars = Variables('config.py', ARGUMENTS)
    vars.AddVariables(
            ('HOST', 'Host machine string for cross-compilation'),
            ('CC', 'The C compiler'),
            ('CCFLAGS', 'C and C++ compilation flags'),
            ('CFLAGS', 'C compilation flags'),
            ('CXXFLAGS', 'C++ compilation flags'))
    return vars

def get_substs(aspects):
    '''
    Obtains a dictionary of substitutions to make in porting.h
    '''

    substs = {}
    for i in ['fs', 'binfmt', 'gltype', 'glwin', 'winsys']:
        name = 'BUGLE_' + i
        value = name + '_' + aspects[i]
        name = name.upper()
        value = value.upper()
        substs[name] = value
    return substs

# Process command line arguments
vars = get_vars()
aspects = setup_aspects()
for name, value in ARGUMENTS.iteritems():
    if name in aspects:
        aspects[name] = value
aspects.Resolve()
aspects.Report()

# Values common to both build environment and host environment
common_kw = dict(
        ENV = os.environ,
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
if aspects['host'] != '':
    common_kw['HOST'] = aspects['host']

setup_options()
if GetOption('bugle_short'):
    for i in ['ALIAS', 'APITABLESC', 'APITABLESH', 'CC', 'CXX', 'LDMODULE', 'LEX', 'LINK', 'SHCC', 'SHCXX', 'SHLINK', 'YACC']:
        common_kw[i + 'COMSTR'] = i + ' $TARGET'
    common_kw['TUCOMSTR'] = 'TU ${TARGETS[1]}'
    common_kw['BUDGIECOMSTR'] = 'BUDGIE ${TARGETS}'

envs = {}
# Environment for tools that have to run on the build machine
envs['build'] = Environment(
        YACCHXXFILESUFFIX = '.h',
        tools = ['lex', 'yacc'],
        **common_kw)

# Environment for the target machine
envs['host'] = CrossEnvironment(
        CPPDEFINES = [
            ('LIBDIR', r'\"%(libdir)s\"' % ac_vars),
            ('PKGLIBDIR', r'\"%(libdir)s/%(PACKAGE)s\"' % ac_vars),
            ('HAVE_CONFIG_H', 1)
            ],
        tools = ['lex', 'yacc', 'subst'],
        **common_kw
        )

# Environment for generating parse tree. This has to be GCC, and in cross
# compilation should be the cross-gcc so that the correct headers are
# identified for the target.
envs['tu'] = CrossEnvironment(
        tools = ['gcc', 'budgie', 'gengl', 'tu', 'subst'],
        BCPATH = [builddir, srcdir],
        **common_kw)

# Post-process environments based on aspects
headers = []
libraries = []
callapi = None

apply_compiler(envs['build'], aspects['build_compiler'], aspects['config'])
apply_compiler(envs['host'], aspects['host_compiler'], aspects['config'])

if aspects['glwin'] == 'wgl':
    headers.extend(['wingdi.h', 'GL/wglext.h'])
    callapi = '__stdcall'
    targets['bugle']['LIBS'].extend(['user32'])
if aspects['glwin'] == 'glx':
    headers.extend(['GL/glx.h', 'GL/glxext.h'])
    targets['bugle']['LIBS'].extend(['X11'])
    targets['bugle']['source'].extend([srcdir.File('src/glx/glxdump.c')])
if aspects['glwin'] == 'egl':
    if aspects['winsys'] == 'windows':
        callapi = '__stdcall'
    headers.extend(['EGL/egl.h', 'EGL/eglext.h'])
    libraries.append('libEGL')

if aspects['gltype'] == 'gl':
    headers.extend(['GL/gl.h', 'GL/glext.h'])
    if aspects['glwin'] == 'wgl':
        libraries.append('opengl32')
    else:
        libraries.append('libGL')
if aspects['gltype'] == 'gles1cm':
    headers.extend(['GLES/gl.h', 'GLES/glext.h'])
    libraries.append('libGLESv1_CM')
if aspects['gltype'] == 'gles2':
    headers.extend(['GLES2/gl2.h', 'GLES2/gl2ext.h'])
    libraries.append('libGLESv2')

if aspects['binfmt'] == 'pe':
    # Can't use LD_PRELOAD on windows, so have to use the same name
    targets['bugle']['target'] = libraries[-1]
else:
    targets['bugle']['target'] = 'libbugle'

# Post-process command-line options
substs = get_substs(aspects)
envs['host'].SubstFile(builddir.File('include/bugle/porting.h'), srcdir.File('include/bugle/porting.h.in'), substs)

envs['host'] = checks(envs['host'], libraries[-1], headers)
Export('envs', 'targets', 'aspects', 'callapi', 'libraries')

# Platform SConscript must be first, since it modifies the environment
# via config.h
# TODO properly detect the platform
SConscript('platform/posix/SConscript')

SConscript('budgie/SConscript')
SConscript('src/SConscript')
SConscript('filters/SConscript')
SConscript('gldb/SConscript')
SConscript('bc/SConscript')

budgie_outputs = [
        'include/budgie/call.h',
        'include/budgie/types2.h',
        'budgielib/defines.h',
        'budgielib/tables.c',
        'budgielib/lib.c'
        ]
envs['tu'].Budgie(budgie_outputs, ['src/data/gl.tu', 'bc/main.bc'])

headers = [envs['tu']['find_header'](envs['host'], h) for h in headers]
# TODO change to bc/alias.bc
envs['tu'].BudgieAlias(target = ['bc/alias.bc'], source = headers)
envs['tu'].ApitablesC(target = ['src/apitables.c'], source = ['budgielib/defines.h'] + headers)
envs['tu'].ApitablesH(target = ['src/apitables.h'], source = ['budgielib/defines.h'] + headers)
