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

from __future__ import print_function, division
import sys
import os.path
import re
import itertools
from collections import OrderedDict
from optparse import OptionParser
from textwrap import dedent
import genglxmltables

try:
    import xml.etree.cElementTree as etree
except ImportError:
    import xml.etree.ElementTree as etree

def sortedDict(d, keyfunc):
    '''
    Takes a dictionary and returns an OrderedDict with the same content,
    sorted by a key function that operates on the values
    '''
    return OrderedDict(sorted(d.items(), key = lambda x: keyfunc(x[1])))

class Enum:
    def __init__(self, elem):
        self.elem = elem
        self.type = elem.get('type', '')
        self.name = elem.get('name')
        self.value = None # A EnumValue
        self.extensions = []

    def key(self):
        newest = min(self.extensions, key = Extension.key)
        return newest.key()

class EnumValue:
    def __init__(self, value):
        self.value = value
        self.enums = []

class Function:
    def __init__(self, elem):
        self.elem = elem
        self.name = elem.find('proto/name').text
        self.extensions = []

class Extension:
    def __init__(self, elem, api):
        self.elem = elem
        self.name = elem.get('name')
        if elem.tag == 'feature':
            self.number = self.elem.get('number')
            self.category = 'VERSION'
        else:
            self.category = self.name.split('_', 2)[1]
            self.number = "0"

        self.enums = []
        self.functions = []
        for require_elem in elem.iterfind('require'):
            if api.match_require(require_elem):
                for enum_elem in require_elem.iterfind('enum'):
                    name = enum_elem.get('name')
                    # Need the test because bit values etc are excluded
                    if name in api.enums:
                        enum = api.enums[name]
                        enum.extensions.append(self)
                        self.enums.append(enum)
                for command_elem in require_elem.iterfind('command'):
                    name = command_elem.get('name')
                    function = api.functions[name]
                    function.extensions.append(self)
                    self.functions.append(function)

    def key(self):
        '''
        Generate a tuple which can be used to sort extensions. Extensions
        are sorted into the order:
         - versions, by decreasing version number
         - Khronos-approved extensions (ARB/KHR/OES)
         - multi-vendor extensions (EXT)
         - vendor extensions
         - extension groups
        '''
        if (self.category == 'VERSION'):
            return (0, -float(self.number))
        elif (self.category == 'ARB' or
              self.category == 'KHR' or
              self.category == 'OES'):
            return (1, self.name)
        elif (self.category == 'EXT'):
            return (2, self.name)
        elif (self.category == 'PSEUDO'):
            return (4, self.name)
        else:
            return (3, self.name)

class PseudoExtension(Extension):
    def __init__(self, name):
        self.elem = None
        self.name = name
        self.category = 'PSEUDO'

class API:
    extra_aliases = []
    extra_tree = etree.XML('<registry></registry>')

    def valid_enums(self, enums_elem):
        return (enums_elem.get('type') != 'bitmask' and
            ('group' not in enums_elem.keys()
                or enums_elem.get('group') not in genglxmltables.bad_enum_groups))

    def __init__(self, filename):
        self.enums = dict()
        self.enum_values = dict()
        self.functions = dict()
        self.extensions = dict()
        tree = etree.parse(filename)
        roots = [self.extra_tree, tree.getroot()]

        for root in roots:
            for enums_elem in root.iterfind('enums'):
                if self.valid_enums(enums_elem):
                    for enum_elem in enums_elem.iterfind('enum'):
                        if not self.match_require(enum_elem):
                            continue
                        enum = Enum(enum_elem)
                        value = int(enum_elem.get('value'), 0)
                        if value not in self.enum_values:
                            self.enum_values[value] = EnumValue(value)
                        enum.value = self.enum_values[value]
                        enum.value.enums.append(enum)
                        assert enum.name not in self.enums
                        self.enums[enum.name] = enum

        for root in roots:
            for command_elem in root.iterfind('commands/command'):
                if not self.match_require(command_elem):
                    continue
                function = Function(command_elem)
                self.functions[function.name] = function

        for root in roots:
            for feature_elem in itertools.chain(
                    root.iterfind('feature'),
                    root.iterfind('extensions/extension')):
                if self.use_extension(feature_elem):
                    extension = Extension(feature_elem, self)
                    self.extensions[extension.name] = extension
        self.extensions = sortedDict(self.extensions, Extension.key)

        # Prune enums and functions that don't apply in this API/profile
        self.enums = {x[0]: x[1] for x in self.enums.items() if x[1].extensions}
        self.functions = {x[0]: x[1] for x in self.functions.items() if x[1].extensions}
        for value in self.enum_values.values():
            value.enums = [x for x in value.enums if x.name in self.enums]
            value.enums.sort(key = Enum.key)
        self.enum_values = {x[0]: x[1] for x in self.enum_values.items() if x[1].enums}

        # Sort enums by value and functions by name
        self.enums = sortedDict(self.enums, lambda x: x.value.value)
        self.enum_values = sortedDict(self.enum_values, lambda x: x.value)
        self.functions = sortedDict(self.functions, lambda x: x.name)

        # Sort extension lists
        for enum in self.enums.values():
            enum.extensions.sort(key = Extension.key)
        for function in self.functions.values():
            function.extensions.sort(key = Extension.key)

    def use_extension(self, extension_elem):
        if extension_elem.tag == 'feature':
            return self.api == extension_elem.get('api')
        else:
            supported_re = re.compile('(?:' + extension_elem.get('supported') + ')$')
            # TODO: needs to consider ES1 required vs extensions
            return supported_re.match(self.default_extensions)

    def match_require(self, elem):
        if 'api' in elem.attrib:
            if elem.get('api') != self.api:
                return False
        if 'profile' in elem.attrib:
            if elem.get('profile') != self.profile:
                return False
        return True

class GLAPI(API):
    api = 'gl'
    profile = 'compatibility'
    default_extensions = 'gl'
    block = 'BUGLE_API_EXTENSION_BLOCK_GL'

class GLES1CMAPI(GLAPI):
    api = 'gles1'
    profile = 'common'
    default_extensions = 'gles1'
    block = 'BUGLE_API_EXTENSION_BLOCK_GL'

class GLES2API(GLAPI):
    api = 'gles2'
    profile = 'common'
    default_extensions = 'gles2'
    block = 'BUGLE_API_EXTENSION_BLOCK_GL'

class GLXAPI(API):
    api = 'glx'
    profile = None
    default_extensions = 'glx'
    block = 'BUGLE_API_EXTENSION_BLOCK_GLWIN'
    extra_aliases = genglxmltables.glx_extra_aliases
    # Add None as a token
    extra_tree = etree.XML('''
        <registry>
            <enums namespace="GLX">
                <enum name="None" value="0"/>
            </enums>
            <feature api="glx" name="GLX_VERSION_1_0" number="1.0">
                <require>
                    <enum name="None" />
                </require>
            </feature>
        </registry>''')

    def valid_enums(self, enums_elem):
        if enums_elem.get('namespace') not in ['GLX', 'GL']:
            return False
        return API.valid_enums(self, enums_elem)

class EGLAPI(API):
    api = 'egl'
    profile = None
    default_extensions = 'egl'
    block = 'BUGLE_API_EXTENSION_BLOCK_GLWIN'

    def valid_enums(self, enums_elem):
        if enums_elem.get('group') == 'Boolean':
            return True
        return API.valid_enums(self, enums_elem)

class WGLAPI(API):
    api = 'wgl'
    profile = None
    default_extensions = 'wgl'
    block = 'BUGLE_API_EXTENSION_BLOCK_GLWIN'
    # WGL has some internal functions not directly called by the user.
    # However, we need to forward them to replace OpenGL32.dll.
    extra_tree = etree.XML(genglxmltables.wgl_extra_xml)

    def valid_enums(self, enums_elem):
        if enums_elem.get('namespace') not in ['WGL', 'GL']:
            return False
        return API.valid_enums(self, enums_elem)

def do_alias(options, apis):
    def generator():
        for api in apis:
            for function in api.functions.values():
                yield function.name
    print('LIMIT', '|'.join(generator()))

    for api in apis:
        for function in api.functions.values():
            aliases = function.elem.findall('alias')
            for a in aliases:
                if a.get('name') in api.functions:
                    print('ALIAS {} {}'.format(function.name, a.get('name')))
        for x, y in api.extra_aliases:
            print('ALIAS {} {}'.format(x, y))

def do_header(options, apis):
    print('/* Generated by ' + sys.argv[0] + '. Do not edit. */')
    print(dedent('''
        #ifndef BUGLE_SRC_APITABLES_H
        #define BUGLE_SRC_APITABLES_H
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
    for api in apis:
        for extension in api.extensions.values():
            print("#define BUGLE_{0} {1}".format(extension.name, index))
            index += 1
    print("#define BUGLE_API_EXTENSION_COUNT {0}".format(index))
    print(dedent("""
        extern const bugle_api_extension_data _bugle_api_extension_table[BUGLE_API_EXTENSION_COUNT];
        extern const bugle_api_function_data _bugle_api_function_table[];
        extern const bugle_api_enum_data * const _bugle_api_enum_table[];
        extern const int _bugle_api_enum_count[];
        #endif /* BUGLE_SRC_APITABLES_H */"""))

def load_function_ids(filename):
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
                functions.append(name)
    return functions

def extension_descendants(extensions, extension):
    ans = [extension]
    q = [extension]
    while q:
        cur = q.pop()
        children = genglxmltables.extension_children.get(cur.name, [])
        for c in children:
            if c in extensions:
                f = extensions[c]
                if f not in ans:
                    ans.append(f)
                    q.append(f)
    return ans

def do_c_extension_table(apis):
    for api in apis:
        for extension in api.extensions.values():
            desc = extension_descendants(api.extensions, extension)
            print(dedent('''
                static const bugle_api_extension extension_group_{0}[] =
                {{''').format(extension.name))
            for d in sorted(desc, key = Extension.key):
                print('    BUGLE_{0},'.format(d.name))
            print('    NULL_EXTENSION')
            print('};')

    print(dedent('''
        const bugle_api_extension_data _bugle_api_extension_table[] =
        {'''))
    for api in apis:
        for extension in api.extensions.values():
            fields = []
            if extension.category == 'VERSION':
                fields.append('"{}"'.format(extension.number))
            else:
                fields.append('NULL')
            fields.append('"{}"'.format(extension.name))
            fields.append('extension_group_{}'.format(extension.name))
            fields.append(api.block)
            print('    {{ {0} }},'.format(', '.join(fields)))
    print('};')

def do_c_function_table(apis, header):
    functions = load_function_ids(header)
    print(dedent('''
        const bugle_api_function_data _bugle_api_function_table[] =
        {'''))
    for function_name in functions:
        function = None
        for api in apis:
            if function_name in api.functions:
                function = api.functions[function_name]
        if function is None:
            print("WARNING: Function {} not found in registry".format(function_name), file = sys.stderr)
            extension = 'NULL_EXTENSION'
        else:
            # TODO: design currently assumes one extension per function
            # We use the oldest extension
            extension = 'BUGLE_' + function.extensions[-1].name
        print('    {{ {0} }},  /* {1} */'.format(extension, function_name))
    print('};')

def do_c_enum_table(apis):
    enum_counts = {}
    for api in apis:
        for enum_value in api.enum_values.values():
            print(dedent('''
                static const bugle_api_extension enum_extensions_{0}[] =
                {{''').format(enum_value.enums[0].name))
            extensions = set()
            for enum in enum_value.enums:
                for extension in enum.extensions:
                    extensions.add(extension)
            for extension in sorted(list(extensions), key = Extension.key):
                print('    BUGLE_{0},'.format(extension.name))
            print('    NULL_EXTENSION')
            print('};')

        print(dedent('''
            static const bugle_api_enum_data _bugle_api_enum_table_{0}[] =
            {{''').format(api.block))
        for enum_value in api.enum_values.values():
            enum = enum_value.enums[0]
            fields = []
            fields.append('%#.4x' % (enum_value.value,))
            fields.append('"' + enum.name + '"')
            fields.append('enum_extensions_' + enum.name)
            print('    {{ {0} }},'.format(', '.join(fields)))
        print('};')
        enum_counts[api.block] = len(api.enum_values)

    print(dedent('''
        const bugle_api_enum_data * const _bugle_api_enum_table[] =
        {
            _bugle_api_enum_table_BUGLE_API_EXTENSION_BLOCK_GL,
            _bugle_api_enum_table_BUGLE_API_EXTENSION_BLOCK_GLWIN
        };'''))
    print(dedent('''
        const int _bugle_api_enum_count[] =
        {{
            {0},
            {1}
        }};''').format(
            enum_counts['BUGLE_API_EXTENSION_BLOCK_GL'],
            enum_counts['BUGLE_API_EXTENSION_BLOCK_GLWIN']))

def do_c(options, apis):
    print('/* Generated by ' + sys.argv[0] + '. Do not edit. */')
    print(dedent('''
        #if HAVE_CONFIG_H
        # include <config.h>
        #endif
        #include <bugle/apireflect.h>
        #include "apitables.h"
        '''))

    do_c_extension_table(apis)
    do_c_function_table(apis, options.header)
    do_c_enum_table(apis)

def main():
    parser = OptionParser()
    parser.add_option('--header', metavar = 'FILE',
                      help = 'header file with function enumeration')
    parser.add_option('-m', '--mode', metavar = 'MODE',
                      type = 'choice', choices = ['c', 'header', 'alias'],
                      help = 'output to generate')
    parser.add_option('--gltype', metavar = 'TYPE',
                      type = 'choice', choices = ['gl', 'gles1cm', 'gles2'],
                      default = 'gl',
                      help = 'GL variant to use for gl.xml')
    parser.set_usage('Usage: %prog [options] -m mode --gltype type gl.xml [...]')
    (options, args) = parser.parse_args()
    if not args:
        parser.print_usage(sys.stderr)
        return 2
    if not options.mode:
        print("Must specify a mode", file = sys.stderr)
        parser.print_usage(sys.stderr)
        return 2
    if options.mode == 'c' and not options.header:
        print("Must specify --header when using C mode")
        parser.print_usage(sys.stderr)
        return 2

    modes = {
        'alias': do_alias,
        'header': do_header,
        'c': do_c
    }

    glfactories = {
        'gl': GLAPI,
        'gles1cm': GLES1CMAPI,
        'gles2': GLES2API
    }

    factories = {
        'gl.xml': glfactories[options.gltype],
        'glx.xml': GLXAPI,
        'egl.xml': EGLAPI,
        'wgl.xml': WGLAPI
    }

    apis = []
    for arg in args:
        base = os.path.basename(arg)
        apis.append(factories[base](arg))
    for key in genglxmltables.extension_children.keys():
        if key.startswith('EXTGROUP_'):
            apis[0].extensions[key] = PseudoExtension(key)
    # Re-sort extensions
    apis[0].extensions = sortedDict(apis[0].extensions, Extension.key)

    modes[options.mode](options, apis)

if __name__ == '__main__':
    main()
