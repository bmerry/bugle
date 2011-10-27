#!/usr/bin/env python
from __future__ import division, print_function
from optparse import OptionParser, OptionValueError
from textwrap import dedent
import re
import sys
import os.path

"""
Generate header and C files for BuGLe from GL and window system headers.

The following data are emitted (depending on command-line options):
- apitables.h:
    - #defines for each extension (versions are considered to be extensions)
    - count of the number of extensions
    - declarations for the data in apitables.c
- apitables.c:
    - list of extensions in each extension group (set of extensions all providing
      some piece of functionality)
    - for each extension:
        - the version name string (or NULL for non-version extensions)
        - the extension name string
        - the group it belongs to
        - the API block it belongs to.
    - for each function:
        - the extension it belongs to.
    - for each enum:
        - the list of extensions that define it.
    - for each API block
        - the enums, with value, canonical name, and the list of extensions that define it
        - the number of enums
- alias.bc:
    - list of aliases between functions with different names but the
      same signatures and semantics.

TODO:
    - handle ES
    - force the WGL internal functions to exist
    - extension chains
    - extension groups
    - alias overrides
"""

class API(object):
    """
    An API (either GL or a window system API).

    This is a base class that only relies on regular expressions to answer
    queries. It is intended to be subclassed so that special cases can be
    handled.
    """

    def __init__(self, headers, block, version_regex, extension_regex, enum_regex, function_regex, default_version):
        """
        Constructor. All the regular expressions should use C{^} and C{$} to
        match an entire string.

        @param headers: Base names of header files defining the API
        @type headers: list
        @param block: C define for the API block owning the extension (e.g. C{'BUGLE_API_EXTENSION_BLOCK_GL'})
        @type block: str
        @param version_regex: Regular expression that matches CPP defines for API versions
        @type version_regex: re.RegexObject
        @param extension_regex: Regular expression that matches CPP defines for
                                API versions or extensions.  It must have a
                                capture group called C{suffix} that captures
                                the suffix that will be found on corresponding
                                function names.
        @type extension_regex: re.RegexObject
        @param function_regex: Regular expression that matches function names (just the name).
        @type function_regex: re.RegexObject
        @param default_version: CPP define for the first supported version of the API. This is
                                assumed when no additional version information is available.
        @type default_version: str
        """
        self.headers = headers
        self.block = block
        self.version_regex = version_regex
        self.extension_regex = extension_regex
        self.enum_regex = enum_regex
        self.function_regex = function_regex

        self.extensions = {}
        self.enums = {}
        self.functions = {}
        self.default_version = self.get_extension(default_version)

    def get_extension(self, extension_name):
        """
        Maps an extension string (CPP define) to an L{Extension} object, creating it if necessary.
        """
        if extension_name not in self.extensions:
            self.extensions[extension_name] = Extension(self, extension_name)
        return self.extensions[extension_name]

    def add_enum(self, name, value, extension):
        """
        Add an enumeration to the API.
        @param name: The CPP define
        @type name: str
        @param value: The defined value
        @type value: int
        @param extension: The extension or version defining the enum
        @type extension: L{Extension}
        """
        if value not in self.enums:
            self.enums[value] = Enum(self, value)
        enum = self.enums[value]
        enum.add_name(name, extension)

    def add_function(self, name, signature, extension):
        """
        Add a declared function to the API.
        @param name: the function name
        @type name: str
        @param signature: the function parameters and return type
        @type signature: return from Function.parse_signature
        @param extension: The extension or version defining the function
        @type extension: L{Extension}
        """
        extension = self.function_extension(name, extension)
        if name in self.functions:
            f = self.functions[name]
            if f.signature != signature:
                raise KeyError("Function `" + name + "' already exists with different signature")
            if f.extension != extension:
                # This happens with Mesa, which doesn't have the #ifndef
                # guards in gl.h. Assume that if one of the two is the
                # default version, then the other is correct.
                if f.extension == self.default_version:
                    f.extension = extension
                elif extension == self.default_version:
                    pass
                else:
                    raise KeyError("Function `" + name + "' specified in two different extensions")
        else:
            self.functions[name] = Function(self, name, signature, extension)

    def is_version(self, s):
        """
        True if s a valid version name.
        """
        return self.version_regex.match(s)

    def get_default_version(self):
        """
        The minimum version of API to which things are assigned if no higher
        version is detected.
        """
        return self.default_version

    def is_extension(self, s):
        """
        True if s is a valid extension name for the API (including a version)
        """
        return self.extension_regex.match(s)

    def extension_suffix(self, extension_name):
        """
        Extracts the extension suffix from an extension name.
        @return: the suffix if found in the extension regex provided to the
                 constructor, otherwise C{''}
        """
        match = self.extension_regex.match(extension_name)
        if match:
            return match.group('suffix')
        else:
            return ''

    def extension_sort_key(self, extension_name):
        """
        Generate a tuple which can be used to sort extensions. Extensions
        are sorted into the order:
         - versions, by version number
         - Khronos-approved extensions (ARB/KHR/OES)
         - multi-vendor extensions (EXT)
         - vendor extensions
         - extension groups
        """
        if extension_name.startswith('EXTGROUP_'):
            return (4, extension_name)
        elif self.is_version(extension_name):
            return (0, tuple(re.findall('\d+', extension_name)))
        else:
            suffix = self.extension_suffix(extension_name)
            if suffix in ['ARB', 'OES', 'KHR']:
                return (1, extension_name)
            elif suffix in ['EXT']:
                return (2, extension_name)
            else:
                return (3, extension_name)

    def is_enum(self, s, value):
        """
        True if s is a value enum name for the block, assuming it has been
        #defined to value.
        """
        return int(value) != 1 and self.enum_regex.match(s)

    def is_function(self, s):
        """
        True if s is a valid function name for the block.
        """
        return self.function_regex.match(s)

    def function_group(self, func):
        """
        Generates a key that will compare equal for all functions that
        alias each other.
        """
        return func.group()

    def function_extension(self, name, extension):
        """
        Determine the correct extension defining function.

        @param name: The function name (must have passed is_function)
        @param extension: The guessed extension (based on #ifndefs)
        """
        return extension

    def extension_children(self, extension_name):
        """
        Return a set of extension names that implement the same functionality
        as the given extension. These children need not exist (they will be
        automatically filtered), and need not include transitive relationships.
        """
        return []

class GLAPI(API):
    """
    API class for the core GL API.

    GL 1.0 is thoroughly dead, so we assume things start with GL 1.1. That is
    the software version provided with old versions of Windows.
    """

    _bad_enum_re = re.compile(r'''^(?:
            GL_GLEXT_.*
            |GL_TRUE
            |GL_FALSE
            |GL_[A-Z0-9_]+_BIT(?:_[A-Z0-9_]+)?
            |GL_(?:[A-Z0-9_]+)?ALL_[A-Z0-9_]+_BITS(?:_[A-Z0-9_]+)?
            |GL_TIMEOUT_IGNORED
            )$''', re.VERBOSE)

    _extension_children = {
        "GL_ATI_draw_buffers": ["GL_ARB_draw_buffers"],
        "GL_ATI_stencil_two_side": ["GL_EXT_stencil_two_side"],
        "GL_ARB_depth_texture": ["GL_VERSION_1_4"],
        "GL_ARB_draw_buffers": ["GL_VERSION_2_0"],
        "GL_ARB_fragment_program": [],
        "GL_ARB_fragment_shader": ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
        "GL_ARB_imaging": [],
        "GL_ARB_map_buffer_range": ["GL_VERSION_3_0"],
        "GL_ARB_multisample": ["GL_VERSION_1_3", "GL_VERSION_ES_CM_1_0", "GL_ES_VERSION_2_0"],
        "GL_ARB_multitexture": ["GL_VERSION_1_3", "GL_VERSION_ES_CM_1_0", "GL_ES_VERSION_2_0"],
        "GL_ARB_occlusion_query": ["GL_VERSION_1_5"],
        "GL_ARB_pixel_buffer_object": ["GL_VERSION_2_1"],
        "GL_ARB_point_parameters": ["GL_VERSION_1_4", "GL_VERSION_ES_CM_1_1", "GL_ES_VERSION_2_0"],
        "GL_ARB_point_sprite": ["GL_VERSION_2_0"],
        "GL_ARB_shader_objects": ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
        "GL_ARB_shading_language_100": ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
        "GL_ARB_shadow": ["GL_VERSION_1_4"],
        "GL_ARB_texture_border_clamp": ["GL_VERSION_1_3"],
        "GL_ARB_texture_compression": ["GL_VERSION_1_3", "GL_VERSION_ES_CM_1_0", "GL_ES_VERSION_2_0"],
        "GL_ARB_texture_cube_map": ["GL_VERSION_1_3", "GL_ES_VERSION_2_0"],
        "GL_ARB_texture_env_add": ["GL_VERSION_1_3", "GL_VERSION_ES_CM_1_0"],
        "GL_ARB_texture_env_dot3": ["GL_VERSION_1_3"],
        "GL_ARB_texture_env_combine": ["GL_VERSION_1_3"],
        "GL_ARB_texture_env_crossbar": ["GL_VERSION_1_4"],
        "GL_ARB_texture_non_power_of_two": ["GL_VERSION_2_0"],
        "GL_ARB_texture_rectangle": [],
        "GL_ARB_transpose_matrix": ["GL_VERSION_1_3"],
        "GL_ARB_vertex_buffer_object": ["GL_VERSION_1_5", "GL_VERSION_ES_CM_1_1", "GL_ARB_vertex_buffer_object"],
        "GL_ARB_vertex_program": [],
        "GL_ARB_vertex_shader": ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
        "GL_ARB_window_pos": ["GL_VERSION_1_4"],
        "GL_EXT_bgra": ["GL_VERSION_1_2"],
        "GL_EXT_bindable_uniform": [],
        "GL_EXT_blend_equation_separate": ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
        "GL_EXT_blend_func_separate": ["GL_VERSION_1_4", "GL_ES_VERSION_2_0"],
        "GL_EXT_draw_range_elements": ["GL_VERSION_1_2"],
        "GL_EXT_fog_coord": ["GL_VERSION_1_4"],
        "GL_EXT_framebuffer_object": ["GL_ES_VERSION_2_0"],
        "GL_EXT_multi_draw_arrays": ["GL_VERSION_1_4"],
        "GL_EXT_packed_pixels": ["GL_VERSION_1_2"],
        "GL_EXT_pixel_buffer_object": ["GL_ARB_pixel_buffer_object"],
        "GL_EXT_point_parameters": ["GL_ARB_point_parameters"],
        "GL_EXT_rescale_normal": ["GL_VERSION_1_2"],
        "GL_EXT_secondary_color": ["GL_VERSION_1_4"],
        "GL_EXT_separate_specular_color": ["GL_VERSION_1_2"],
        "GL_EXT_shadow_funcs": ["GL_VERSION_1_5"],
        "GL_EXT_stencil_two_side": ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
        "GL_EXT_stencil_wrap": ["GL_VERSION_1_4", "GL_ES_VERSION_2_0"],
        "GL_EXT_texture3D": ["GL_VERSION_1_2"],
        "GL_EXT_texture_buffer_object": [],
        "GL_EXT_texture_cube_map": ["GL_ARB_texture_cube_map"],
        "GL_EXT_texture_env_combine": ["GL_ARB_texture_env_combine"],
        "GL_EXT_texture_filter_anisotropic": [],
        "GL_EXT_texture_lod_bias": ["GL_VERSION_1_4"],
        "GL_EXT_texture_rectangle": ["GL_ARB_texture_rectangle"],
        "GL_EXT_texture_sRGB": ["GL_VERSION_2_1"],
        "GL_SGIS_generate_mipmap": ["GL_VERSION_1_4"],
        "GL_SGIS_texture_edge_clamp": ["GL_VERSION_1_2", "GL_ES_VERSION_2_0"],
        "GL_SGIS_texture_lod": ["GL_VERSION_1_2"],
        "GL_NV_blend_square": ["GL_VERSION_1_4", "GL_ES_VERSION_2_0"],
        "GL_NV_texture_rectangle": ["GL_EXT_texture_rectangle"],

        # This got promoted to core from imaging subset in 1.4
        "EXTGROUP_blend_color": ["GL_EXT_blend_color", "GL_ARB_imaging", "GL_VERSION_1_4"],
        # GL_ARB_vertex_program and GL_ARB_fragment_program have a lot of overlap
        "EXTGROUP_old_program": ["GL_ARB_vertex_program", "GL_ARB_fragment_program"],
        # Extensions that define GL_MAX_TEXTURE_IMAGE_UNITS and GL_MAX_TEXTURE_COORDS
        "EXTGROUP_texunits": ["GL_ARB_fragment_program", "GL_ARB_vertex_shader", "GL_ARB_fragment_shader", "GL_VERSION_2_0"],
        # Extensions that have GL_VERTEX_PROGRAM_POINT_SIZE and GL_VERTEX_PROGRAM_TWO_SIDE
        "EXTGROUP_vp_options": ["GL_ARB_vertex_program", "GL_ARB_vertex_shader", "GL_VERSION_2_0"],
        # Extensions that define generic vertex attributes
        "EXTGROUP_vertex_attrib": ["GL_ARB_vertex_program", "GL_ARB_vertex_shader", "GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
        # Extensions that define FramebufferTextureLayerEXT - needed because some
        # versions of Mesa headers do odd things with this function
        "EXTGROUP_framebuffer_texture_layer": ["GL_EXT_texture_array", "GL_NV_geometry_program4"]
    }

    def __init__(self):
        API.__init__(self,
            ['gl.h', 'glext.h'],
            'BUGLE_API_EXTENSION_BLOCK_GL',
            re.compile(r'^GL_(?:ES_)?VERSION_(?:ES_)?[0-9_]+$'),
            re.compile(r'^GL_(?P<suffix>[0-9A-Z]+)_\w+$'),
            re.compile(r'^GL_[0-9A-Za-z_]+$'),
            re.compile(r'^gl[A-WY-Z]\w+$'),  # Being careful to avoid glX
            'GL_VERSION_1_1')
        # Force the extension groups to exist
        for e in self._extension_children.keys():
            self.get_extension(e)

    def is_extension(self, s):
        if s.startswith('GL_GLEXT_'):
            return False
        else:
            return API.is_extension(self, s)

    def is_enum(self, s, value):
        if self._bad_enum_re.match(s):
            return False
        else:
            return API.is_enum(self, s, value)

    def function_extension(self, name, extension):
        if name == 'glFramebufferTextureLayerEXT':
            return self.get_extension('EXTGROUP_framebuffer_texture_layer')
        else:
            return API.function_extension(self, name, extension)

    def function_group(self, func):
        # Some extra aliases that can't be automatically detected.
        extra_alias = [
            ('glCreateShaderObjectARB',          'glCreateShader'),
            ('glCreateProgramObjectARB',         'glCreateProgram'),
            ('glUseProgramObjectARB',            'glUseProgram'),
            ('glAttachObjectARB',                'glAttachShader'),
            ('glDetachObjectARB',                'glDetachShader'),
            ('glGetAttachedObjectsARB',          'glGetAttachedShaders'),
            ('glXCreateContextWithConfigSGIX',   'glXCreateNewContext'),
            ('glXMakeCurrentReadSGI',            'glXMakeContextCurrent')
        ]
        group = API.function_group(self, func)
        for e in extra_alias:
            if func.name == e[0]:
                group = (e[1], group[1:])
                break
        return group

    def extension_children(self, extension_name):
        return self._extension_children.get(extension_name, [])

class WGLAPI(API):
    """
    API class for the WGL API.

    WGL has no API spec and is not versioned, so we invent WGL_VERSION_1_0
    as a 'version'.
    """
    def __init__(self):
        API.__init__(self,
            ['wingdi.h', 'wglext.h'],
            "BUGLE_API_EXTENSION_BLOCK_GLWIN",
            re.compile(r'^WGL_VERSION_[0-9_]+$'),
            re.compile(r'^WGL_(?P<suffix>[0-9A-Z]+)_\w+$'),
            re.compile(r'^WGL_[0-9A-Za-z_]+$'),
            re.compile(r'^(?:wgl[A-Z]\w+|Glmf|glDebugEntry)$'),
            'WGL_VERSION_1_0')

class EGLAPI(API):
    """
    API class for the EGL API.

    Although EGL has versions 1.0-1.2, they were never widely deployed, so
    we start with EGL 1.3.
    """
    def __init__(self):
        API.__init__(self,
            ['egl.h', 'eglext.h'],
            'BUGLE_API_EXTENSION_BLOCK_GLWIN',
            re.compile(r'^EGL_VERSION_[0-9_]+$'),
            re.compile(r'^EGL_(?P<suffix>[0-9A-Z]+)_\w+$'),
            re.compile(r'^EGL_[0-9A-Za-z_]+$'),
            re.compile(r'^egl[A-Z]\w+$'),
            'EGL_VERSION_1_3')

class GLXAPI(API):
    """
    API class for the GLX API.

    Although GLX has versions 1.0-1.1, they are seldom seen in the wild, so
    we start with GLX 1.2.
    """

    _extension_children = {
        "GLX_ARB_get_proc_address": ["GLX_VERSION_1_4"],
        "GLX_EXT_import_context": ["GLX_VERSION_1_3"],
        "GLX_SGI_make_current_read": ["GLX_VERSION_1_3"],
        "GLX_SGIX_fbconfig": ["GLX_VERSION_1_3"]
    }

    def __init__(self):
        API.__init__(self,
            ['glx.h', 'glxext.h'],
            'BUGLE_API_EXTENSION_BLOCK_GLWIN',
            re.compile(r'^GLX_VERSION_[0-9_]+$'),
            re.compile(r'^GLX_(?P<suffix>[0-9A-Z]+)_\w+$'),
            re.compile(r'^GLX_[0-9A-Za-z_]+$'),
            re.compile(r'^glX[A-Z]\w+$'),
            'GLX_VERSION_1_2')

    def is_extension(self, s):
        if s.startswith('GLX_GLXEXT_'):
            return False
        else:
            return API.is_extension(self, s)

    def function_extension(self, name, extension):
        # glXGetCurrentDisplay is in GLX 1.2, but in glxext.h it is
        # incorrectly listed in the header block for 1.3.
        if name == 'glXGetCurrentDisplay':
            return self.get_extension('GLX_VERSION_1_2')
        else:
            return API.function_extension(self, name, extension)

    def extension_children(self, extension_name):
        return self._extension_children.get(extension_name, [])

class Extension(object):
    """
    Encapsulates a GL or window system version or extension.
    """

    _extensions = {}

    def __init__(self, api, name):
        self.api = api
        self.name = name
        self.children = set()

    def get_name(self):
        return self.name

    def is_version(self):
        return self.api.is_version(self.name)

    def get_suffix(self):
        return self.api.extension_suffix(self.name)

    @classmethod
    def get_extension(cls, api, name):
        if name not in cls._extensions:
            cls._extensions[name] = Extension(api, name)
        return cls._extensions[name]

    def add_child(self, child):
        """
        Indicate that a child extension subsumes the functionality of this one.
        """
        children.add(child)

    def get_descendants(self):
        """
        Find all extensions that have the functionality defined by this one.

        @rtype: list of Extensions
        """
        q = [self]
        ans = []
        while q:
            cur = q.pop()
            if cur not in ans:
                ans.append(cur)
                q.extend(cur.children)
        return ans

class Enum(object):
    """
    An API-specific enumerant. There is only one instance of this
    object for each enum value, using a canonical name.

    @ivar name: canonical name for the value
    @type name: str
    @ivar extensions: maps each name to the extension that defines it
    @type extensions: dictionary mapping str to L{Extension}
    """

    # TODO: move this regex into the API objects
    _unwanted_name = re.compile('''^(?:
        GL_ATTRIB_ARRAY_SIZE_NV
        |GL_ATTRIB_ARRAY_STRIDE_NV
        |GL_ATTRIB_ARRAY_TYPE_NV
        |GL_ATTRIB_ARRAY_POINTER_NV
        |GL_PIXEL_COUNT_AVAILABLE_NV
        |GL_PIXEL_COUNT_NV
        |GL_CURRENT_OCCLUSION_QUERY_ID_NV
        |GL_PIXEL_COUNTER_BITS_NV
        |GL_TEXTURE_COMPONENTS
        |GL_CURRENT_ATTRIB_NV
        |GL_MAP._VERTEX_ATTRIB._._NV

        |GL_NO_ERROR
        |GL_ZERO
        |GL_ONE

        |GLX_BAD.*
        |GLX_HYPERPIPE_.*
        |GLX_PIPE_RECT_.*
        |GLX_SYNC_SWAP_.*
        |GLX_SYNC_FRAME_.*
        |GLX_3DFX_.*_MODE_.*
        )$''', re.VERBOSE);


    def __init__(self, api, value):
        self.name = None
        self.api = api
        self.value = value
        self.extensions = {}  

    @classmethod
    def _name_key(cls, name):
        '''
        Compute a sort key such that more preferred names
        have a lower key.
        '''
        if cls._unwanted_name.match(name):
            return (1, name)
        else:
            return (0, name)

    def add_name(self, name, extension):
        """
        Add a CPP define for this enum value.
        @param name: CPP define
        @type name: str
        @param extension: extension defining the enum
        @type extension: L{Extension}
        """
        if self.name is None or self._name_key(name) < self._name_key(self.name):
            self.name = name
        if name in self.extensions:
            if extension != self.extensions[name]:
                # Normally two extensions can't define the same name, but
                # Mesa's gl.h defines things outside of #ifndef blocks which
                # makes them appear to be in the default extension.
                if self.extensions[name] == self.api.default_version:
                    self.extensions[name] = extension
                elif extension != self.api.default_version:
                    raise ValueError("%s defined by both %s and %s" % (name, self.extensions[name], extension))
        else:
            self.extensions[name] = extension

    def add_extension(self, extension):
        self.extensions.add(extension)

    def get_extensions(self):
        return self.extensions

class Function(object):
    """
    Encapsulation of a function.

    There is one of these objects for each function name. Equivalent
    sets of functions with different names have different objects.
    TODO: create a function group class?
    """

    _split_arg_re = re.compile(r'\s+|(\*|\[|\])', re.DOTALL)
    _split_params_re = re.compile(r',\s*', re.DOTALL)
    _type_re = re.compile(r'^[A-Za-z_][A-Za-z0-9_]*$')

    def __init__(self, api, name, signature, extension):
        self.api = api
        self.name = name
        self.signature = signature
        self.extension = extension

    @classmethod
    def _stripargname(cls, arg):
        """
        Extracts just the type from a named parameter.

        >>> Function._stripargname('const GLfloat m[16]')
        'const GLfloat *'
        >>> Function._stripargname('const GLfloat foo')
        'const GLfloat'
        >>> Function._stripargname('const GLfloat')
        'const GLfloat'
        >>> Function._stripargname('void * const *x')
        'void * const *'
        >>> Function._stripargname('GLvoid **y')
        'GLvoid * *'
        >>> Function._stripargname('const int * foo')
        'const int *'
        >>> Function._stripargname('const int *foo_bar')
        'const int *'
        """
        # GL headers have only a subset of the full C range of
        # parameters.
        tokens = []
        tokens = cls._split_arg_re.split(arg)
        tokens = [x for x in tokens if x is not None and x != '']
        has_type = False
        out = []
        for t in tokens:
            if t != 'const' and cls._type_re.match(t):
                if not has_type:
                    out.append(t)
                    has_type = True
                else:
                    pass
            else:
                out.append(t)
        # Convert type foo[n] to type *foo
        if len(out) >= 3 and out[-3] == '[' and out[-1] == ']':
            out = out[0:-3] + ['*']
        return ' '.join(out)

    @classmethod
    def parse_signature(cls, match):
        ret = match.group('ret')
        args = match.group('args')
        args = cls._split_params_re.split(args)
        if args == ('void'):
            args = ()
        args = [cls._stripargname(x) for x in args]
        ret = ret.strip()
        return (ret, tuple(args))

    def base_name(self):
        """
        The name of the function with any extension suffix removed.
        """
        suffix = self.extension.get_suffix()
        if suffix and self.name.endswith(suffix):
            return self.name[:-len(suffix)]
        else:
            return self.name

    def group(self):
        """
        Return a key indicating which group the function belongs to.
        All functions with the same key must have the same
        semantics.
        """
        return (self.base_name(), self.api, self.signature)

class LineHandler(object):
    """
    Class that attempts to match a line in the header file and dispatch
    it to a suitable function.
    """
    def __init__(self, regex, handler):
        if isinstance(regex, str):
            self.regex = re.compile(regex, re.DOTALL)
        else:
            self.regex = regex
        self.handler = handler

    def handle(self, line):
        '''
        Handle a line of input.

        @return: Whether the line was consumed
        '''
        match = self.regex.match(line)
        if match:
            return self.handler(match, line)
        else:
            return False

class State(object):
    def __init__(self):
        self.api = None
        self.extension = None

    def handle_ifndef(self, match, line):
        name = match.group('name')
        if self.api.is_extension(name):
            self.extension = self.api.get_extension(name)
            return True
        return False

    def handle_endif(self, match, line):
        self.extension = self.api.default_version

    def handle_define(self, match, line):
        name = match.group('name')
        try:
            value = int(match.group('value'), 0)
        except ValueError:
            return False
        if self.api.is_enum(name, value):
            self.api.add_enum(name, value, self.extension)
            return True
        return False

    def handle_function(self, match, line):
        if line.startswith('typedef '):
            return
        name = match.group('name')
        if self.api.is_function(name):
            signature = Function.parse_signature(match)
            self.api.add_function(name, signature, self.extension)
            return True
        return False

def load_headers(filenames):
    apis = [GLAPI(), GLXAPI(), WGLAPI(), EGLAPI()]
    state = State()
    line_handlers = [
        LineHandler(r'#ifndef (?P<name>\w+?)(?:_DEPRECATED)?\s*$', state.handle_ifndef),
        LineHandler(r'#endif', state.handle_endif),
        LineHandler(r'#define (?P<name>\w+)\s+(?P<value>(?:0x)?[0-9A-Fa-f]+)', state.handle_define),
        LineHandler(r'(?:(?:[A-Z]*API[A-Z]*|extern)\s+)?(?P<ret>[A-Za-z0-9_ *]+?)\b\s*(?:[A-Z]*API[A-Z]*\s+)?(?P<name>[a-z_][A-Za-z0-9_]*)\s*\((?P<args>.*)\)', state.handle_function)
    ]
    for filename in filenames:
        basename = os.path.basename(filename)
        state.api = None
        for api in apis:
            if basename in api.headers:
                state.api = api
                break
        if state.api is None:
            raise KeyError('Unknown header file "' + basename + "'")
        state.extension = state.api.default_version
        with open(filename, 'r') as hf:
            vline = ''
            for line in hf:
                vline += line
                # Function prototypes may be split over multiple lines
                # To parse them correctly, keep consuming lines until
                # parentheses are matched
                if vline.count('(') == vline.count(')'):
                    for handler in line_handlers:
                        if handler.handle(vline):
                            break
                    vline = ''

    # Filter out APIs that weren't actually used
    apis = [api for api in apis if len(api.functions) > 0]
    return apis

def load_function_ids(filename, apis):
    func_re = re.compile('^#define FUNC_(\w+)\s+(\d+)')
    functions = []
    with open(filename, 'r') as h:
        for line in h:
            match = func_re.match(line)
            if match:
                name = match.group(1)
                fid = int(match.group(2))
                if fid != len(functions):
                    raise ValueError("Expected function ID of " + len(functions) + " but got " + fid)
                func = None
                for api in apis:
                    for (n, f) in api.functions.items():
                        if n == name:
                            func = f
                            break
                if func is None:
                    raise KeyError("Function name " + name + " has not been parsed")
                functions.append(func)
    return functions

def process_alias(apis, options):
    for api in apis:
        groups = {}
        for f in api.functions.values():
            group = api.function_group(f)
            if group not in groups:
                groups[group] = [f]
            else:
                groups[group].append(f)
        for group in groups.values():
            for f in group[1:]:
                print("ALIAS %s %s" % (f.name, group[0].name))

def process_header(apis, functions, options):
    print('/* Generated by ' + sys.argv[0] + '. Do not edit. */')
    print(dedent('''
        #ifndef BUGLE_SRC_GLTABLES_H
        #define BUGLE_SRC_GLTABLES_H
        #ifdef HAVE_CONFIG_H
        # include <config.h>
        #endif
        #include <bugle/apireflect.h>

        typedef struct
        {
            const char *version;
            const char *name;
            const bugle_api_extension *group;
            api_block block;
        } bugle_api_extension_data;

        typedef struct
        {
            GLenum value;
            const char *name;
            const bugle_api_extension *extensions;
        } bugle_api_enum_data;

        typedef struct
        {
            bugle_api_extension extension;
        } bugle_api_function_data;
        '''))
    index = 0
    # TODO: anything special needed for extension groups?
    for api in apis:
        for extension in sorted(api.extensions.keys(), key = api.extension_sort_key):
            print("#define BUGLE_{} {}".format(extension, index))
            index += 1
    print("#define BUGLE_API_EXTENSION_COUNT {}".format(index))
    print(dedent("""
        extern const bugle_api_extension_data _bugle_api_extension_table[BUGLE_API_EXTENSION_COUNT];
        extern const bugle_api_function_data _bugle_api_function_table[];
        extern const bugle_api_enum_data * const _bugle_api_enum_table[];
        extern const int _bugle_api_enum_count[];
        #endif /* BUGLE_SRC_APITABLES_H */"""))

def main():
    parser = OptionParser()
    parser.add_option('--header', dest = 'header', metavar = 'FILE',
                      help = 'header file with function enumeration')
    parser.add_option('-m', '--mode', dest = 'mode', metavar = 'MODE',
                      type = 'choice', choices = ['c', 'header', 'alias'],
                      help = 'output to generate')
    parser.set_usage('Usage: %prog [options] headers')
    (options, args) = parser.parse_args()
    if not args:
        parser.print_usage(sys.stderr)
        return 2

    apis = load_headers(args)
    functions = None
    if not options.mode:
        parser.error('--mode is required')
    if options.header:
        functions = load_function_ids(options.header, apis)
    elif options.mode != 'alias':
        parser.error('--header is required when not in alias mode')

    if options.mode == 'alias':
        process_alias(apis, options)
    elif options.mode == 'c':
        process_c(apis, functions, options)
    elif options.mode == 'header':
        process_header(apis, functions, options)

if __name__ == '__main__':
    import doctest
    doctest.testmod()

    sys.exit(main())
