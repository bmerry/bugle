#!/usr/bin/env python
import sys
import os
from SCons.Script.Main import OptionsParser
from CrossEnvironment import CrossEnvironment
from Aspects import *
import BugleChecks

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

def subdir(srcdir, dir, **kw):
    '''
    Run SConscript in a subdir, setting srcdir appropriately
    '''
    SConscript(dirs = [dir], exports = {'srcdir': srcdir.Dir(dir)}, **kw)

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

def callapi_default(aspect):
    winsys = aspects['winsys']
    if winsys == 'windows':
        return '__stdcall'
    else:
        return ''

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
            env.AppendUnique(CCFLAGS = ['-g'])
        elif config == 'release':
            env.AppendUnique(CCFLAGS = ['-O2'], LINKFLAGS = ['-s'])
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
        name = 'callapi',
        help = 'Calling convention for GL functions',
        default = callapi_default))
    aspects.AddAspect(Aspect(
        name = 'config',
        help = 'Compiler option set',
        choices = ['debug', 'release'],
        default = 'debug'))

    aspects.AddAspect(Aspect(
        name = 'prefix',
        help = 'Installation prefix',
        default = '/usr/local')),
    aspects.AddAspect(Aspect(
        name = 'libdir',
        help = 'Installation path for libraries',
        default = lambda a: aspects['prefix'] + '/lib')),
    aspects.AddAspect(Aspect(
        name = 'pkglibdir',
        help = 'Installation path for plugins',
        default = lambda a: aspects['libdir'] + '/bugle')),
    aspects.AddAspect(Aspect(
        name = 'bindir',
        help = 'Installation path for binaries',
        default = lambda a: aspects['prefix'] + '/lib'))

    aspects.AddAspect(Aspect(
        name = 'CC',
        help = 'C compiler',
        default = None))
    aspects.AddAspect(Aspect(
        name = 'CCFLAGS',
        help = 'C and C++ compilation flags',
        default = None))
    aspects.AddAspect(Aspect(
        name = 'CFLAGS',
        help = 'C compilation flags',
        default = None))
    aspects.AddAspect(Aspect(
        name = 'CXXFLAGS',
        help = 'C++ compilation flags',
        default = None))
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

# Process command line arguments
aspects = setup_aspects()
for name, value in ARGUMENTS.iteritems():
    if name in aspects:
        aspects[name] = value
    else:
        raise RuntimeError, name + ' is not a valid option'
aspects.Resolve()
aspects.Report()

# Values common to both build environment and host environment
common_kw = dict(
        ENV = os.environ,
        CPPPATH = [Dir('.')])
        # toolpath = ['%(srcdir)s/site_scons/site_tools' % ac_vars],

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
            ('LIBDIR', r'\"%s\"' % aspects['libdir']),
            ('PKGLIBDIR', r'\"%s\"' % aspects['pkglibdir']),
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
        **common_kw)

# Post-process environments based on aspects
headers = []
libraries = []

apply_compiler(envs['build'], aspects['build_compiler'], aspects['config'])
apply_compiler(envs['host'], aspects['host_compiler'], aspects['config'])

for env in envs.itervalues():
    if aspects['host'] != '':
        env['HOST'] = aspects['host']
    # single values
    for i in ['CC']:
        if aspects[i] is not None and aspects[i] != '':
            env[i] = aspects[i]
    # Lists
    for i in ['CFLAGS', 'CCFLAGS', 'CXXFLAGS']:
        if aspects[i] is not None and aspects[i] != '':
            env.AppendUnique(**{i: Split(aspects[i])})

if aspects['glwin'] == 'wgl':
    headers.extend(['wingdi.h', 'GL/wglext.h'])
    targets['bugle']['LIBS'].extend(['user32'])
if aspects['glwin'] == 'glx':
    headers.extend(['GL/glx.h', 'GL/glxext.h'])
    targets['bugle']['LIBS'].extend(['X11'])
    targets['bugle']['source'].extend(['#src/glx/glxdump.c'])
if aspects['glwin'] == 'egl':
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

headers = [envs['tu']['find_header'](envs['host'], h) for h in headers]

envs['host'] = checks(envs['host'], libraries[-1], headers)
Export('envs', 'targets', 'aspects', 'headers', 'libraries', 'subdir')

subdir(Dir('.'), 'src', variant_dir = 'build', duplicate = 0)
