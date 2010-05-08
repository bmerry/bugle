#!/usr/bin/env python
import os
from BugleAspects import *

version = '0.0.20100509'

def subdir(srcdir, dir, **kw):
    '''
    Run SConscript in a subdir, setting srcdir appropriately
    '''
    SConscript(dirs = [dir], exports = {'srcdir': srcdir.Dir(dir)}, **kw)

def host_compiler_default(aspect):
    if DefaultEnvironment()['PLATFORM'] == 'win32':
        return 'mingw'
    else:
        return 'gcc'

def platform_default(aspect):
    compiler = aspects['host-compiler']
    if compiler == 'msvc' or compiler == 'mingw':
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
    AddOption('--sticky', help ='Save build options for later runs', dest = 'bugle_save', action = 'store_true')

def setup_aspects():
    '''
    Creates an AspectParser object with the user-configurable options
    '''
    aspects = AspectParser()
    aspects.AddAspect(Aspect(
        group = 'variant',
        name = 'build-compiler',
        help = 'Compiler for tools used to build the rest of bugle',
        choices = ['gcc', 'mingw', 'msvc'],
        default = 'gcc'))
    aspects.AddAspect(Aspect(
        group = 'variant',
        name = 'host-compiler',
        help = 'Compiler for the final outputs',
        choices = ['gcc', 'mingw', 'msvc'],
        default = host_compiler_default))
    aspects.AddAspect(Aspect(
        group = 'variant',
        name = 'host',
        help = 'Name for the host machine for cross-compiling (GCC only)',
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
        group = 'subvariant',
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
        group = 'subvariant',
        name = 'binfmt',
        help = 'Dynamic linker style',
        choices = ['pe', 'elf'],
        default = binfmt_default))
    aspects.AddAspect(Aspect(
        group = 'subvariant',
        name = 'callapi',
        help = 'Calling convention for GL functions',
        default = callapi_default))
    aspects.AddAspect(Aspect(
        group = 'variant',
        name = 'config',
        help = 'Compiler option set',
        choices = ['debug', 'release'],
        default = 'release'))

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
        group = 'path',
        name = 'includedir',
        help = 'Installation path for header files',
        default = lambda a: aspects['prefix'] + '/include'))
    aspects.AddAspect(Aspect(
        group = 'path',
        name = 'datadir',
        help = 'Installation path for architecture-independent data',
        default = lambda a: aspects['prefix'] + '/share'))
    aspects.AddAspect(Aspect(
        group = 'path',
        name = 'docdir',
        help = 'Installation path for documentation',
        default = lambda a: aspects['datadir'] + '/doc/bugle'))

    aspects.AddAspect(Aspect(
        group = 'package',
        name = 'without_gtk',
        help = 'Prevent GTK+ from being detected and used',
        choices = ['yes', 'no'],
        default = 'no'))
    aspects.AddAspect(Aspect(
        group = 'package',
        name = 'without_gtkglext',
        help = 'Prevent GtkGLExt from being detected and used',
        choices = ['yes', 'no'],
        default = 'no'))
    aspects.AddAspect(Aspect(
        group = 'package',
        name = 'without_lavc',
        help = 'Prevent ffmpeg/libavcodec from being detected and used',
        choices = ['yes', 'no'],
        default = 'no'))

    # TODO separate specification of build CC and host CC
    aspects.AddAspect(Aspect(
        group = 'tool',
        name = 'CC',
        help = 'C compiler',
        default = None))
    aspects.AddAspect(Aspect(
        group = 'tool',
        name = 'CXX',
        help = 'C++ compiler',
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
    aspects.AddAspect(Aspect(
        group = 'flags',
        name = 'LINKFLAGS',
        help = 'C and C++ linking flags',
        default = None))

    aspects.AddAspect(Aspect(
        group = 'parts',
        name = 'parts',
        help = 'Select which parts of BuGLe to build',
        choices = ['debugger', 'interceptor', 'docs', 'tests'],
        multiple = True,
        default = 'all'))

    return aspects

# Process command line arguments
setup_options()
aspects = setup_aspects()
Help('Available command-line variables:\n\n')
Help(aspects.Help())
if GetOption('help'):
    Return()
aspects.LoadFile('config.py')
aspects.Load(ARGUMENTS, 'command line')
aspects.Resolve()

variant = []
for a in aspects.ordered:
    if a.get() is not None and a.get() != '':
        if a.group == 'variant' or (a.group == 'subvariant' and a.explicit):
            variant.append('%s' % a.get())
variant_str = '_'.join(variant)

package_sources = []
PACKAGEROOT = Dir('bugle-' + version)
Export('aspects', 'subdir', 'version', 'package_sources', 'PACKAGEROOT')

subdir(Dir('.'), 'src', variant_dir = 'build/' + variant_str, duplicate = 0)
subdir(Dir('.'), 'doc/DocBook', variant_dir = 'build/doc', duplicate = 0)

package_env = Environment(tools = ['default', 'packaging'])
package_sources.extend([
    'AUTHORS',
    'COPYING',
    'ChangeLog',
    'LICENSE',
    'NEWS',
    'README',
    'SConstruct',
    'TODO-SCons',

    Glob('site_scons/*.py'),
    Glob('site_scons/site_tools/*.py'),

    Glob('doc/*.txt'),
    Glob('doc/*.html'),
    'doc/examples/filters',
    'doc/examples/statistics',

    Glob('src/gl2ps/*'),
    ])
for i in [
        '*.c', '*.h', '*.y', '*.l', '*.cpp', '*.ll', '*.yy',
        '*.def', '*.py', '*.perl', '*.bc', '*.in',
        '*.xml', '*.xsl', '*.css', '*.ent', '*.png',
        'SConscript']:
    for levels in ['', '*/', '*/*/', '*/*/*/', '*/*/*/*/']:
        package_sources += Glob('doc/' + levels + i)
        package_sources += Glob('src/' + levels + i)

package_env.Package(source = package_sources,
                    NAME = 'bugle',
                    VERSION = version,
                    PACKAGEVERSION = 0,
                    PACKAGETYPE = 'src_tarbz2',
                    # TODO: share with encoding in src/SConscript
                    PACKAGEROOT = PACKAGEROOT,
                    LICENCE = 'gpl',
                    SUMMARY = 'testing')

# Only save right at the end, in case some subdir rejects an aspect
# However, we save what the user asks for, not what they got, in case
# the modifications change later (e.g. they install a package)
aspects.Report()
if GetOption('bugle_save'):
    aspects.Save('config.py')
