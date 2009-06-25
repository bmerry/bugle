#!/usr/bin/env python
import os
from Aspects import *

def subdir(srcdir, dir, **kw):
    '''
    Run SConscript in a subdir, setting srcdir appropriately
    '''
    SConscript(dirs = [dir], exports = {'srcdir': srcdir.Dir(dir)}, **kw)

def host_compiler_default(aspect):
    if DefaultEnvironment()['PLATFORM'] == 'win32':
        return 'msvc'
    else:
        return 'gcc'

def platform_default(aspect):
    compiler = aspects['host-compiler']
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
        group = 'variant',
        name = 'build-compiler',
        help = 'Compiler for tools used to build the rest of bugle',
        choices = ['gcc', 'msvc'],
        default = 'gcc'))
    aspects.AddAspect(Aspect(
        group = 'variant',
        name = 'host-compiler',
        help = 'Compiler for the final outputs',
        choices = ['gcc', 'msvc'],
        default = host_compiler_default))
    aspects.AddAspect(Aspect(
        group = 'variant',
        name = 'host',
        help = 'name for the host machine for cross-compiling',
        default = ''))

    aspects.AddAspect(Aspect(
        group = 'variant',
        name = 'gltype',
        help = 'Variant of GL',
        choices = ['gl', 'gles1cm', 'gles2'],
        default = 'gl'))
    aspects.AddAspect(Aspect(
        group = 'variant',
        name = 'glwin',
        help = 'Window system abstraction',
        choices = ['wgl', 'glx', 'egl'],
        default = glwin_default))
    aspects.AddAspect(Aspect(
        group = 'variant',
        name = 'winsys',
        help = 'Windowing system',
        choices = ['windows', 'x11', 'none'],
        default = winsys_default))
    aspects.AddAspect(Aspect(
        group = 'variant',
        name = 'fs',
        help = 'Location of files',
        choices = ['cygming', 'unix'],
        default = fs_default))
    aspects.AddAspect(Aspect(
        group = 'variant',
        name = 'platform',
        help = 'C runtime libraries',
        choices = ['posix', 'msvcrt', 'null'],
        default = platform_default))
    aspects.AddAspect(Aspect(
        group = 'variant',
        name = 'binfmt',
        help = 'Dynamic linker style',
        choices = ['pe', 'elf'],
        default = binfmt_default))
    aspects.AddAspect(Aspect(
        group = 'variant',
        name = 'callapi',
        help = 'Calling convention for GL functions',
        default = callapi_default))
    aspects.AddAspect(Aspect(
        group = 'variant',
        name = 'config',
        help = 'Compiler option set',
        choices = ['debug', 'release'],
        default = 'debug'))

    aspects.AddAspect(Aspect(
        group = 'path',
        name = 'prefix',
        help = 'Installation prefix',
        default = '/usr/local')),
    aspects.AddAspect(Aspect(
        group = 'path',
        name = 'libdir',
        help = 'Installation path for libraries',
        default = lambda a: aspects['prefix'] + '/lib')),
    aspects.AddAspect(Aspect(
        group = 'path',
        name = 'pkglibdir',
        help = 'Installation path for plugins',
        default = lambda a: aspects['libdir'] + '/bugle')),
    aspects.AddAspect(Aspect(
        group = 'path',
        name = 'bindir',
        help = 'Installation path for binaries',
        default = lambda a: aspects['prefix'] + '/bin'))

    aspects.AddAspect(Aspect(
        group = 'tool',
        name = 'CC',
        help = 'C compiler',
        default = None))
    aspects.AddAspect(Aspect(
        group = 'flags',
        name = 'CCFLAGS',
        help = 'C and C++ compilation flags',
        default = None))
    aspects.AddAspect(Aspect(
        group = 'flags',
        name = 'CFLAGS',
        help = 'C compilation flags',
        default = None))
    aspects.AddAspect(Aspect(
        group = 'flags',
        name = 'CXXFLAGS',
        help = 'C++ compilation flags',
        default = None))
    return aspects

# Process command line arguments
setup_options()
aspects = setup_aspects()
for name, value in ARGUMENTS.iteritems():
    if name in aspects:
        aspects[name] = value
    else:
        raise RuntimeError, name + ' is not a valid option'
aspects.Resolve()
aspects.Report()

variant = []
for a in aspects.ordered:
    if a.group == 'variant' and (not a.is_default() or a.name == 'config'):
        variant.append('%s_%s' % (a.name, a.get()))
variant_str = '__'.join(variant)

Export('aspects', 'subdir')

subdir(Dir('.'), 'src', variant_dir = 'build/' + variant_str, duplicate = 0)
