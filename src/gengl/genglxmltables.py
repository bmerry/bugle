#!/usr/bin/env python3

# BuGLe: an OpenGL debugging tool
# Copyright (C) 2013-2014  Bruce Merry
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

extension_children = {
    "GL_APPLE_flush_buffer_range": ["GL_ARB_map_buffer_range"],
    "GL_ATI_draw_buffers": ["GL_ARB_draw_buffers"],
    "GL_AMD_draw_buffers_blend": ["GL_ARB_draw_buffers_blend"],
    "GL_ARB_ES2_compatibility": ["GL_VERSION_4_1"],
    "GL_ARB_blend_func_extended": ["GL_VERSION_3_3"],
    "GL_ARB_color_buffer_float": ["GL_VERSION_3_0"],
    "GL_ARB_depth_buffer_float": ["GL_VERSION_3_0"],
    "GL_ARB_depth_clamp": ["GL_VERSION_3_2"],
    "GL_ARB_depth_texture": ["GL_VERSION_1_4"],
    "GL_ARB_draw_buffers": ["GL_VERSION_2_0"],
    "GL_ARB_draw_buffers_blend": ["GL_VERSION_4_0"],
    "GL_ARB_draw_elements_base_vertex": ["GL_VERSION_3_2"],
    "GL_ARB_draw_indirect": ["GL_VERSION_4_0"],
    "GL_ARB_draw_instanced": ["GL_VERSION_3_1"],
    "GL_ARB_explicit_attrib_location": ["GL_VERSION_3_3"],
    "GL_ARB_fragment_coord_conventions": ["GL_VERSION_3_2"],
    "GL_ARB_fragment_program": [],
    "GL_ARB_fragment_shader": ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
    "GL_ARB_framebuffer_object": ["GL_VERSION_3_0"],
    "GL_ARB_framebuffer_sRGB": ["GL_VERSION_3_0"],
    "GL_ARB_get_program_binary": ["GL_VERSION_4_1"],
    "GL_ARB_geometry_shader4": ["GL_VERSION_3_2"],
    "GL_ARB_gpu_shader5": ["GL_VERSION_4_0"],
    "GL_ARB_gpu_shader_fp64": ["GL_VERSION_4_0"],
    "GL_ARB_half_float_pixel": ["GL_VERSION_3_0"],
    "GL_ARB_half_float_vertex": ["GL_VERSION_3_0"],
    "GL_ARB_imaging": [],
    "GL_ARB_instanced_arrays": ["GL_VERSION_3_3"],
    "GL_ARB_map_buffer_range": ["GL_VERSION_3_0"],
    "GL_ARB_multisample": ["GL_VERSION_1_3", "GL_VERSION_ES_CM_1_0", "GL_ES_VERSION_2_0"],
    "GL_ARB_multitexture": ["GL_VERSION_1_3", "GL_VERSION_ES_CM_1_0", "GL_ES_VERSION_2_0"],
    "GL_ARB_occlusion_query": ["GL_VERSION_1_5"],
    "GL_ARB_occlusion_query2": ["GL_VERSION_3_3"],
    "GL_ARB_pixel_buffer_object": ["GL_VERSION_2_1"],
    "GL_ARB_point_parameters": ["GL_VERSION_1_4", "GL_VERSION_ES_CM_1_1", "GL_ES_VERSION_2_0"],
    "GL_ARB_point_sprite": ["GL_VERSION_2_0"],
    "GL_ARB_provoking_vertex": ["GL_VERSION_3_2"],
    "GL_ARB_sample_shading": ["GL_VERSION_4_0"],
    "GL_ARB_sampler_objects": ["GL_VERSION_3_3"],
    "GL_ARB_seamless_cube_map": ["GL_VERSION_3_2"],
    "GL_ARB_separate_shader_objects": ["GL_VERSION_4_1"],
    "GL_ARB_shader_bit_encoding": ["GL_VERSION_3_3"],
    "GL_ARB_shader_objects": ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
    "GL_ARB_shader_precision": ["GL_VERSION_4_1"],
    "GL_ARB_shader_subroutine": ["GL_VERSION_4_0"],
    "GL_ARB_shader_texture_lod": ["GL_VERSION_3_0"],
    "GL_ARB_shading_language_100": ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
    "GL_ARB_shadow": ["GL_VERSION_1_4"],
    "GL_ARB_sync": ["GL_VERSION_3_2"],
    "GL_ARB_tessellation_shader": ["GL_VERSION_4_0"],
    "GL_ARB_texture_border_clamp": ["GL_VERSION_1_3"],
    "GL_ARB_texture_buffer_object": ["GL_VERSION_3_1"],
    "GL_ARB_texture_buffer_object_rgb32": ["GL_VERSION_4_0"],
    "GL_ARB_texture_compression": ["GL_VERSION_1_3", "GL_VERSION_ES_CM_1_0", "GL_ES_VERSION_2_0"],
    "GL_ARB_texture_compression_rgtc": ["GL_VERSION_3_0"],
    "GL_ARB_texture_compression_bptc": ["GL_VERSION_4_1"],
    "GL_ARB_texture_cube_map": ["GL_VERSION_1_3", "GL_ES_VERSION_2_0"],
    "GL_ARB_texture_cube_map_array": ["GL_VERSION_4_0"],
    "GL_ARB_texture_env_add": ["GL_VERSION_1_3", "GL_VERSION_ES_CM_1_0"],
    "GL_ARB_texture_env_dot3": ["GL_VERSION_1_3"],
    "GL_ARB_texture_env_combine": ["GL_VERSION_1_3"],
    "GL_ARB_texture_env_crossbar": ["GL_VERSION_1_4"],
    "GL_ARB_texture_float": ["GL_VERSION_3_0"],
    "GL_ARB_texture_gather": ["GL_VERSION_4_0"],
    "GL_ARB_texture_mirrored_repeat": ["GL_VERSION_1_4"],
    "GL_ARB_texture_multisample": ["GL_VERSION_3_2"],
    "GL_ARB_texture_non_power_of_two": ["GL_VERSION_2_0"],
    "GL_ARB_texture_query_lod": ["GL_VERSION_4_0"],
    "GL_ARB_texture_rectangle": ["GL_VERSION_3_1"],
    "GL_ARB_texture_rg": ["GL_VERSION_3_0"],
    "GL_ARB_texture_rgb10_a2ui": ["GL_VERSION_3_3"],
    "GL_ARB_texture_swizzle": ["GL_VERSION_3_3"],
    "GL_ARB_timer_query": ["GL_VERSION_3_3"],
    "GL_ARB_transform_feedback2": ["GL_VERSION_4_0"],
    "GL_ARB_transform_feedback3": ["GL_VERSION_4_0"],
    "GL_ARB_transpose_matrix": ["GL_VERSION_1_3"],
    "GL_ARB_uniform_buffer_object": ["GL_VERSION_3_1"],
    "GL_ARB_vertex_array_object": ["GL_VERSION_3_0"],
    "GL_ARB_vertex_attrib_64bit": ["GL_VERSION_4_1"],
    "GL_ARB_vertex_array_bgra": ["GL_VERSION_3_2"],
    "GL_ARB_vertex_buffer_object": ["GL_VERSION_1_5", "GL_VERSION_ES_CM_1_1", "GL_ARB_vertex_buffer_object"],
    "GL_ARB_vertex_program": [],
    "GL_ARB_vertex_shader": ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
    "GL_ARB_vertex_type_2_10_10_10_rev": ["GL_VERSION_3_3"],
    "GL_ARB_viewport_array": ["GL_VERSION_4_1"],
    "GL_ARB_window_pos": ["GL_VERSION_1_4"],
    "GL_EXT_bgra": ["GL_VERSION_1_2"],
    "GL_EXT_bindable_uniform": [],
    "GL_EXT_blend_equation_separate": ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
    "GL_EXT_blend_func_separate": ["GL_VERSION_1_4", "GL_ES_VERSION_2_0"],
    "GL_EXT_draw_range_elements": ["GL_VERSION_1_2"],
    "GL_EXT_fog_coord": ["GL_VERSION_1_4"],
    "GL_EXT_framebuffer_blit": ["GL_ARB_framebuffer_object"],
    "GL_EXT_framebuffer_object": ["GL_ES_VERSION_2_0", "GL_ARB_framebuffer_object"],
    "GL_EXT_framebuffer_multisample": ["GL_ARB_framebuffer_object"],
    # Note: although ARB_framebuffer_sRGB incorporates equivalent functionality,
    # the GL_FRAMEBUFFER_SRGB_CAPABLE query was dropped so it is not a strict subset.
    "GL_EXT_framebuffer_sRGB": [],
    "GL_EXT_multi_draw_arrays": ["GL_VERSION_1_4"],
    "GL_EXT_packed_depth_stencil": ["GL_VERSION_3_0"],
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
    "GL_EXT_texture_array": ["GL_VERSION_3_0"],
    "GL_EXT_texture_buffer_object": [],
    "GL_EXT_texture_cube_map": ["GL_ARB_texture_cube_map"],
    "GL_EXT_texture_env_combine": ["GL_ARB_texture_env_combine"],
    "GL_EXT_texture_filter_anisotropic": [],
    "GL_EXT_texture_integer": ["GL_VERSION_3_0"],
    "GL_EXT_texture_lod_bias": ["GL_VERSION_1_4"],
    "GL_EXT_texture_sRGB": ["GL_VERSION_2_1"],
    "GL_EXT_texture_shared_exponent": ["GL_VERSION_3_0"],
    "GL_EXT_transform_feedback": ["GL_VERSION_3_0"],
    "GL_SGIS_generate_mipmap": ["GL_VERSION_1_4"],
    "GL_SGIS_texture_edge_clamp": ["GL_VERSION_1_2", "GL_ES_VERSION_2_0"],
    "GL_SGIS_texture_lod": ["GL_VERSION_1_2"],
    "GL_NV_blend_square": ["GL_VERSION_1_4", "GL_ES_VERSION_2_0"],
    "GL_NV_depth_buffer_float": ["GL_ARB_depth_buffer_float"],
    "GL_NV_packed_depth_stencil": ["GL_EXT_packed_depth_stencil"],
    "GL_NV_texture_rectangle": ["GL_ARB_texture_rectangle"],

    "GLX_ARB_get_proc_address": ["GLX_VERSION_1_4"],
    "GLX_EXT_import_context": ["GLX_VERSION_1_3"],
    "GLX_SGI_make_current_read": ["GLX_VERSION_1_3"],
    "GLX_SGIX_fbconfig": ["GLX_VERSION_1_3"],

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
    # TODO: instead use the registry to decide which extensions provide this function
    "EXTGROUP_framebuffer_texture_layer": ["GL_EXT_geometry_shader4", "GL_EXT_texture_array", "GL_NV_geometry_program4"]
}

# These aliases are not in the registry because the underlying protocol is different
glx_extra_aliases = [
    ('glXChooseFBConfigSGIX', 'glXChooseFBConfig'),
    ('glXCreateContextWithConfigSGIX', 'glXCreateNewContext'),
    ('glXGetCurrentDisplayEXT', 'glXGetCurrentDisplay'),
    ('glXGetCurrentReadDrawableSGI', 'glXGetCurrentReadDrawable'),
    ('glXGetProcAddressARB', 'glXGetProcAddress'),
    ('glXMakeCurrentReadSGI', 'glXMakeContextCurrent'),
    ('glXSelectEventSGIX', 'glXSelectEvent')
]

# Extension enum groups that contain tokens that alias other tokens and are
# more likely to cause problems than to give correct dumps, at least until
# support is written for these extensions.
bad_enum_groups = [
    'TriangleListSUN',
    'MapTextureFormatINTEL',
    'PathRenderingTokenNV',
    'GLXAttribute'
]

wgl_extra_xml = '''
<registry>
    <commands namespace="WGL">
        <command>
            <proto>int <name>wglChoosePixelFormat</name></proto>
            <param><ptype>HDC</ptype> <name>hdc</name></param>
            <param>const <ptype>PIXELFORMATDESCRIPTOR</ptype> *<name>ppfd</name></param>
        </command>
        <command>
            <proto><ptype>BOOL</ptype> <name>wglSetPixelFormat</name></proto>
            <param><ptype>HDC</ptype> <name>hdc</name></param>
            <param>int <name>ipfd</name></param>
            <param><ptype>UINT</ptype> <name>cjpfd</name></param>
            <param>const <ptype>PIXELFORMATDESCRIPTOR</ptype> *<name>ppfd</name></param>
        </command>
        <command>
            <proto>int <name>wglDescribePixelFormat</name></proto>
            <param><ptype>HDC</ptype> <name>hdc</name></param>
            <param>int <name>ipfd</name></param>
            <param><ptype>UINT</ptype> <name>cjpfd</name></param>
            <param><ptype>PIXELFORMATDESCRIPTOR</ptype> *<name>ppfd</name></param>
        </command>
        <command>
            <proto>int <name>wglGetPixelFormat</name></proto>
            <param><ptype>HDC</ptype> <name>hdc</name></param>
        </command>
        <command>
            <proto><ptype>PROC</ptype> <name>wglGetDefaultProcAddress</name></proto>
            <param><ptype>HDC</ptype> <name>hdc</name></param>
        </command>
        <command>
            <proto><ptype>BOOL</ptype> <name>wglSwapBuffers</name></proto>
            <param><ptype>HDC</ptype> <name>hdc</name></param>
        </command>
        <command>
            <proto><ptype>DWORD</ptype> <name>wglSwapMultipleBuffers</name></proto>
            <param><ptype>UINT</ptype> <name>a1</name></param>
            <!-- Actually CONST WGLSWAP *, but WGLSWAP isn't defined by the registry -->
            <param>const void *<name>a2</name></param>
        </command>
        <command>
            <proto>int <name>GlmfPlayGlsRecord</name></proto>
            <param><ptype>DWORD</ptype> a1</param>
            <param><ptype>DWORD</ptype> a2</param>
            <param><ptype>DWORD</ptype> a3</param>
            <param><ptype>DWORD</ptype> a4</param>
        </command>
        <command>
            <proto>int <name>GlmfInitPlayback</name></proto>
            <param><ptype>DWORD</ptype> a1</param>
            <param><ptype>DWORD</ptype> a2</param>
            <param><ptype>DWORD</ptype> a3</param>
        </command>
        <command>
            <proto>int <name>GlmfEndPlayback</name></proto>
            <param><ptype>DWORD</ptype> a1</param>
        </command>
        <command>
            <proto>int <name>GlmfEndGlsBlock</name></proto>
            <param><ptype>DWORD</ptype> a1</param>
        </command>
        <command>
            <proto>int <name>GlmfCloseMetaFile</name></proto>
            <param><ptype>DWORD</ptype> a1</param>
        </command>
        <command>
            <proto>int <name>GlmfBeginGlsBlock</name></proto>
            <param><ptype>DWORD</ptype> a1</param>
        </command>
        <command>
            <proto>int <name>glDebugEntry</name></proto>
            <param><ptype>DWORD</ptype> a1</param>
            <param><ptype>DWORD</ptype> a2</param>
        </command>
    </commands>

    <feature api="wgl" name="WGL_VERSION_1_0" number="1.0">
        <require>
            <command name="wglChoosePixelFormat"/>
            <command name="wglSetPixelFormat"/>
            <command name="wglDescribePixelFormat"/>
            <command name="wglGetPixelFormat"/>
            <command name="wglGetDefaultProcAddress"/>
            <command name="wglSwapBuffers"/>
            <command name="wglSwapMultipleBuffers"/>
            <command name="GlmfPlayGlsRecord"/>
            <command name="GlmfInitPlayback"/>
            <command name="GlmfEndPlayback"/>
            <command name="GlmfEndGlsBlock"/>
            <command name="GlmfCloseMetaFile"/>
            <command name="GlmfBeginGlsBlock"/>
            <command name="glDebugEntry"/>
        </require>
    </feature>
</registry>
'''
