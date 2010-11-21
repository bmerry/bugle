#!/usr/bin/env python
import os
from BugleAspects import *

version = '0.0.20101121'

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
    if compiler == 'msvc':
        return 'msvcrt'
    elif compiler == 'mingw':
        return 'mingw'
    else:
        return 'posix'

def winsys_default(aspect):
    platform = aspects['platform']
    if platform == 'msvcrt' or platform == 'mingw':
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
    if platform == 'msvcrt' or platform == 'mingw':
        return 'cygming'    # TODO add windows fs
    else:
        return 'unix'

def binfmt_default(aspect):
    platform = aspects['platform']
    if platform == 'msvcrt' or platform == 'mingw':
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
        choices = ['posix', 'mingw', 'msvcrt', 'null'],
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
        group = 'path',
        name = 'mandir',
        help = 'Installation path for manual pages',
        default = lambda a: aspects['datadir'] + '/man'))

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
        group = 'tool',
        name = 'XSLTPROC',
        help = 'XSLT processor',
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
        group = 'flags',
        name = 'CPPPATH',
        help = 'Colon-separated list of paths to find headers',
        default = None))
    aspects.AddAspect(Aspect(
        group = 'flags',
        name = 'LIBPATH',
        help = 'Colon-separated list of paths to find libraries',
        default = None))

    aspects.AddAspect(Aspect(
        group = 'options',
        name = 'parts',
        help = 'Select which parts of BuGLe to build',
        choices = ['debugger', 'interceptor', 'docs', 'tests'],
        multiple = True,
        default = 'all'))
    aspects.AddAspect(Aspect(
        group = 'options',
        name = 'quiet',
        help = 'Show abbreviated command-lines',
        choices = ['yes', 'no'],
        default = 'yes'))

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
Default('build')

Alias('install', FindInstalledFiles())

package_env = Environment(tools = ['default', 'packaging'], TAR = 'fakeroot tar')
package_sources.extend([
    'AUTHORS',
    'COPYING',
    'ChangeLog',
    'LICENSE',
    'NEWS',
    'README',
    'SConstruct',
    'doc/DocBook/SConscript',
    'doc/DocBook/bugle.css',
    'doc/DocBook/bugle.ent',
    'doc/DocBook/bugle.xml',
    'doc/DocBook/changelog.xml',
    'doc/DocBook/extending.xml',
    'doc/DocBook/faq.xml',
    'doc/DocBook/gles.xml',
    'doc/DocBook/hacking.xml',
    'doc/DocBook/images/gtk-copy.png',
    'doc/DocBook/images/gtk-zoom-100.png',
    'doc/DocBook/images/gtk-zoom-fit.png',
    'doc/DocBook/images/gtk-zoom-in.png',
    'doc/DocBook/images/gtk-zoom-out.png',
    'doc/DocBook/install.xml',
    'doc/DocBook/introduction.xml',
    'doc/DocBook/manpages/bugle.xml',
    'doc/DocBook/manpages/camera.xml',
    'doc/DocBook/manpages/checks.xml',
    'doc/DocBook/manpages/eps.xml',
    'doc/DocBook/manpages/error.xml',
    'doc/DocBook/manpages/exe.xml',
    'doc/DocBook/manpages/extoverride.xml',
    'doc/DocBook/manpages/frontbuffer.xml',
    'doc/DocBook/manpages/gldb-gui.xml',
    'doc/DocBook/manpages/log.xml',
    'doc/DocBook/manpages/logstats.xml',
    'doc/DocBook/manpages/manpages.xml',
    'doc/DocBook/manpages/screenshot.xml',
    'doc/DocBook/manpages/showerror.xml',
    'doc/DocBook/manpages/showextensions.xml',
    'doc/DocBook/manpages/showstats.xml',
    'doc/DocBook/manpages/statistics.xml',
    'doc/DocBook/manpages/stats_basic.xml',
    'doc/DocBook/manpages/stats_calls.xml',
    'doc/DocBook/manpages/stats_calltimes.xml',
    'doc/DocBook/manpages/stats_fragments.xml',
    'doc/DocBook/manpages/stats_nv.xml',
    'doc/DocBook/manpages/stats_primitives.xml',
    'doc/DocBook/manpages/trace.xml',
    'doc/DocBook/manpages/unwindstack.xml',
    'doc/DocBook/manpages/wireframe.xml',
    'doc/DocBook/manpages.xsl',
    'doc/DocBook/protocol.xml',
    'doc/DocBook/releasenotes.xml',
    'doc/DocBook/xhtml-chunk-php.xsl',
    'doc/DocBook/xhtml-chunk.xsl',
    'doc/DocBook/xhtml-common.xsl',
    'doc/DocBook/xhtml-local.xsl',
    'doc/DocBook/xhtml-php.xsl',
    'doc/DocBook/xhtml-single.xsl',
    'doc/examples/filters',
    'doc/examples/statistics',
    'doc/extensions.txt',
    'doc/filters.html',
    'doc/locks.txt',
    'doc/porting.txt',
    'site_scons/BugleAspects.py',
    'site_scons/BugleChecks.py',
    'site_scons/BugleTarget.py',
    'site_scons/CrossEnvironment.py',
    'site_scons/site_tools/budgie.py',
    'site_scons/site_tools/buglewrappers.py',
    'site_scons/site_tools/docbook.py',
    'site_scons/site_tools/gengl.py',
    'site_scons/site_tools/subst.py',
    'site_scons/site_tools/tu.py',
    'src/SConscript',
    'src/apireflect.c',
    'src/bc/SConscript',
    'src/bc/egl/egl.bc',
    'src/bc/gl/GL_ARB_draw_elements_base_vertex.bc',
    'src/bc/gl/GL_ARB_imaging.bc',
    'src/bc/gl/GL_ARB_map_buffer_range.bc',
    'src/bc/gl/GL_ARB_multitexture.bc',
    'src/bc/gl/GL_ARB_occlusion_query.bc',
    'src/bc/gl/GL_ARB_shader_objects.bc',
    'src/bc/gl/GL_ARB_texture_compression.bc',
    'src/bc/gl/GL_ARB_transpose_matrix.bc',
    'src/bc/gl/GL_ARB_vertex_buffer_object.bc',
    'src/bc/gl/GL_ARB_vertex_program.bc',
    'src/bc/gl/GL_ATI_draw_buffers.bc',
    'src/bc/gl/GL_EXT_blend_func_separate.bc',
    'src/bc/gl/GL_EXT_draw_buffers2.bc',
    'src/bc/gl/GL_EXT_draw_instanced.bc',
    'src/bc/gl/GL_EXT_draw_range_elements.bc',
    'src/bc/gl/GL_EXT_framebuffer_object.bc',
    'src/bc/gl/GL_EXT_geometry_shader4.bc',
    'src/bc/gl/GL_EXT_gpu_program_parameters.bc',
    'src/bc/gl/GL_EXT_multi_draw_arrays.bc',
    'src/bc/gl/GL_EXT_secondary_color.bc',
    'src/bc/gl/GL_EXT_texture3D.bc',
    'src/bc/gl/GL_NV_transform_feedback.bc',
    'src/bc/gl/core1_1.bc',
    'src/bc/gl/gl.bc',
    'src/bc/gl/glsl1_20.bc',
    'src/bc/gles1cm/gles1cm.bc',
    'src/bc/gles2/gles2.bc',
    'src/bc/glx/GLX_ARB_get_proc_address.bc',
    'src/bc/glx/GLX_SGIX_fbconfig.bc',
    'src/bc/glx/GLX_SGIX_pbuffer.bc',
    'src/bc/glx/GLX_SGI_make_current_read.bc',
    'src/bc/glx/glx.bc',
    'src/bc/main.bc.in',
    'src/bc/wgl/wgl.bc',
    'src/budgie/SConscript',
    'src/budgie/bclexer.ll',
    'src/budgie/bcparser.yy',
    'src/budgie/budgie.cpp',
    'src/budgie/budgie.h',
    'src/budgie/c-common.def',
    'src/budgie/tree.cpp',
    'src/budgie/tree.def',
    'src/budgie/tree.h',
    'src/budgie/treeutils.cpp',
    'src/budgie/treeutils.h',
    'src/budgie/tulexer.ll',
    'src/budgielib/addresses.c',
    'src/budgielib/internal.c',
    'src/budgielib/internal.h',
    'src/budgielib/lib.h',
    'src/budgielib/reflect.c',
    'src/bugle.pc.in',
    'src/common/hashtable.c',
    'src/common/io-impl.h',
    'src/common/io.c',
    'src/common/linkedlist.c',
    'src/common/memory.c',
    'src/common/protocol-win32.c',
    'src/common/protocol.c',
    'src/common/protocol.h',
    'src/common/workqueue.h',
    'src/common/workqueue.c',
    'src/conffile.h',
    'src/conflex.l',
    'src/confparse.y',
    'src/data/gl.c',
    'src/egl/egl-win.c',
    'src/egl/glwin.c',
    'src/filters.c',
    'src/filters/SConscript',
    'src/filters/camera.c',
    'src/filters/checks.c',
    'src/filters/common.c',
    'src/filters/debugger.c',
    'src/filters/eps.c',
    'src/filters/exe.c',
    'src/filters/extoverride.c',
    'src/filters/logstats.c',
    'src/filters/modify.c',
    'src/filters/screenshot.c',
    'src/filters/showextensions.c',
    'src/filters/showstats.c',
    'src/filters/stats_basic.c',
    'src/filters/stats_calls.c',
    'src/filters/stats_calltimes.c',
    'src/filters/stats_fragments.c',
    'src/filters/stats_nv.c',
    'src/filters/stats_primitives.c',
    'src/filters/trace.c',
    'src/filters/unwindstack.c',
    'src/filters/validate.c',
    'src/gengl/find_header.perl',
    'src/gengl/genegldef.perl',
    'src/gengl/gengl.perl',
    'src/gl/glbeginend.c',
    'src/gl/gldisplaylist.c',
    'src/gl/gldump.c',
    'src/gl/glextensions.c',
    'src/gl/globjects.c',
    'src/gl/glsl.c',
    'src/gl/glstate.c',
    'src/gl/gltypes.c',
    'src/gl/glutils.c',
    'src/gl/opengl32.def',
    'src/gl2ps/COPYING.GL2PS',
    'src/gl2ps/COPYING.LGPL',
    'src/gl2ps/TODO',
    'src/gl2ps/gl2ps.c',
    'src/gl2ps/gl2ps.h',
    'src/gl2ps/gl2ps.pdf',
    'src/gl2ps/gl2psTest.c',
    'src/gl2ps/gl2psTestSimple.c',
    'src/gldb/SConscript',
    'src/gldb/gldb-channels.c',
    'src/gldb/gldb-channels.h',
    'src/gldb/gldb-common.c',
    'src/gldb/gldb-common.h',
    'src/gldb/gldb-gui-backtrace.c',
    'src/gldb/gldb-gui-backtrace.h',
    'src/gldb/gldb-gui-breakpoint.c',
    'src/gldb/gldb-gui-breakpoint.h',
    'src/gldb/gldb-gui-buffer.c',
    'src/gldb/gldb-gui-buffer.h',
    'src/gldb/gldb-gui-framebuffer.c',
    'src/gldb/gldb-gui-framebuffer.h',
    'src/gldb/gldb-gui-image.c',
    'src/gldb/gldb-gui-image.h',
    'src/gldb/gldb-gui-shader.c',
    'src/gldb/gldb-gui-shader.h',
    'src/gldb/gldb-gui-state.c',
    'src/gldb/gldb-gui-state.h',
    'src/gldb/gldb-gui-target.c',
    'src/gldb/gldb-gui-target.h',
    'src/gldb/gldb-gui-texture.c',
    'src/gldb/gldb-gui-texture.h',
    'src/gldb/gldb-gui.c',
    'src/gldb/gldb-gui.h',
    'src/gles1cm/gldump.c',
    'src/gles1cm/glstate.c',
    'src/gles2/gldump.c',
    'src/gles2/glstate.c',
    'src/glwin/glwintypes.c',
    'src/glwin/trackcontext.c',
    'src/glx/glwin.c',
    'src/glx/glxdump.c',
    'src/include/SConscript',
    'src/include/budgie/addresses.h',
    'src/include/budgie/basictypes.h',
    'src/include/budgie/macros.h',
    'src/include/budgie/reflect.h',
    'src/include/budgie/types.h',
    'src/include/bugle/apireflect.h',
    'src/include/bugle/attributes.h',
    'src/include/bugle/bool.h',
    'src/include/bugle/egl/glwintypes.h',
    'src/include/bugle/egl/overrides.h',
    'src/include/bugle/export.h',
    'src/include/bugle/filters.h',
    'src/include/bugle/gl/glbeginend.h',
    'src/include/bugle/gl/gldisplaylist.h',
    'src/include/bugle/gl/gldump.h',
    'src/include/bugle/gl/glextensions.h',
    'src/include/bugle/gl/glheaders.h',
    'src/include/bugle/gl/globjects.h',
    'src/include/bugle/gl/glsl.h',
    'src/include/bugle/gl/glstate.h',
    'src/include/bugle/gl/gltypes.h',
    'src/include/bugle/gl/glutils.h',
    'src/include/bugle/gl/overrides.h',
    'src/include/bugle/glwin/glwin.h',
    'src/include/bugle/glwin/glwintypes.h',
    'src/include/bugle/glwin/trackcontext.h',
    'src/include/bugle/glx/glxdump.h',
    'src/include/bugle/glx/overrides.h',
    'src/include/bugle/hashtable.h',
    'src/include/bugle/input.h',
    'src/include/bugle/io.h',
    'src/include/bugle/linkedlist.h',
    'src/include/bugle/log.h',
    'src/include/bugle/math.h',
    'src/include/bugle/memory.h',
    'src/include/bugle/objects.h',
    'src/include/bugle/porting.h.in',
    'src/include/bugle/stats.h',
    'src/include/bugle/string.h',
    'src/include/bugle/time.h',
    'src/include/bugle/wgl/overrides.h',
    'src/input.c',
    'src/interceptor.c',
    'src/log.c',
    'src/objects.c',
    'src/platform/cosf_pass.c',
    'src/platform/cosf_soft.c',
    'src/platform/dl.h',
    'src/platform/dl_dlopen.c',
    'src/platform/dl_loadlibrary.c',
    'src/platform/dl_null.c',
    'src/platform/gettime_null.c',
    'src/platform/gettime_posix.c',
    'src/platform/gettime_queryperformancecounter.c',
    'src/platform/io_null.c',
    'src/platform/io_posix.c',
    'src/platform/io_msvcrt.c',
    'src/platform/isfinite_msvcrt.c',
    'src/platform/isfinite_null.c',
    'src/platform/isfinite_pass.c',
    'src/platform/isnan_msvcrt.c',
    'src/platform/isnan_pass.c',
    'src/platform/isnan_soft.c',
    'src/platform/mingw/SConscript',
    'src/platform/mingw/platform/io.h',
    'src/platform/mingw/platform/macros.h',
    'src/platform/mingw/platform/threads.h',
    'src/platform/mingw/platform/types.h',
    'src/platform/msvcrt/SConscript',
    'src/platform/msvcrt/platform/io.h',
    'src/platform/msvcrt/platform/macros.h',
    'src/platform/msvcrt/platform/threads.h',
    'src/platform/msvcrt/platform/types.h',
    'src/platform/nan_soft.c',
    'src/platform/nan_strtod.c',
    'src/platform/null/platform/io.h',
    'src/platform/null/platform/macros.h',
    'src/platform/null/platform/threads.h',
    'src/platform/null/platform/types.h',
    'src/platform/posix/SConscript',
    'src/platform/posix/dlopen.c',
    'src/platform/posix/platform/io.h',
    'src/platform/posix/platform/macros.h',
    'src/platform/posix/platform/threads.h',
    'src/platform/posix/platform/types.h',
    'src/platform/round_pass.c',
    'src/platform/round_soft.c',
    'src/platform/sinf_pass.c',
    'src/platform/sinf_soft.c',
    'src/platform/strdup_msvcrt.c',
    'src/platform/strdup_pass.c',
    'src/platform/strndup_soft.c',
    'src/platform/threads_msvcrt.c',
    'src/platform/threads_posix.c',
    'src/platform/vasprintf_msvcrt.c',
    'src/platform/vasprintf_null.c',
    'src/platform/vasprintf_pass.c',
    'src/platform/vasprintf_vsnprintf.c',
    'src/platform/vsnprintf_msvcrt.c',
    'src/platform/vsnprintf_null.c',
    'src/platform/vsnprintf_pass.c',
    'src/stats.c',
    'src/statslex.l',
    'src/statsparse.y',
    'src/tests/SConscript',
    'src/tests/dlopen.c',
    'src/tests/errors.c',
    'src/tests/extoverride.c',
    'src/tests/filters',
    'src/tests/interpose.c',
    'src/tests/math.c',
    'src/tests/objects.c',
    'src/tests/pbo.c',
    'src/tests/pointers.c',
    'src/tests/procaddress.c',
    'src/tests/queries.c',
    'src/tests/setstate.c',
    'src/tests/shadertest.c',
    'src/tests/showextensions.c',
    'src/tests/statistics',
    'src/tests/string.c',
    'src/tests/test.c',
    'src/tests/test.h',
    'src/tests/texcomplete.c',
    'src/tests/textest.c',
    'src/tests/threads.c',
    'src/tests/threads1.c',
    'src/tests/threads2.c',
    'src/tests/triangles.c',
    'src/wgl/glwin.c'])

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
