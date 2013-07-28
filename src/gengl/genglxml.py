#!/usr/bin/env python3
import sys
import os.path
import re
from collections import OrderedDict
from optparse import OptionParser
from textwrap import dedent
from lxml import etree
import genglxmltables

class Enum:
    def __init__(self, elem):
        self.elem = elem
        self.type = elem.get('type', '')
        self.name = elem.get('name')
        self.extensions = []

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
            if api.matchRequire(require_elem):
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
        else:
            return (3, self.name)

class API:
    def __init__(self, filename):
        self.tree = etree.parse(filename)
        self.enums = dict()
        self.enum_values = dict()
        self.functions = dict()
        self.extensions = OrderedDict()
        tree = etree.parse(filename)
        root = tree.getroot()
        for enums_elem in root.iterfind('enums'):
            if enums_elem.get('type') != 'bitmask' and 'group' not in enums_elem:
                for enum_elem in enums_elem.iterfind('enum'):
                    enum = Enum(enum_elem)
                    value = int(enum_elem.get('value'), 0)
                    if value not in self.enum_values:
                        self.enum_values[value] = EnumValue(value)
                    enum.value = self.enum_values[value]
                    enum.value.enums.append(enum)
                    self.enums[enum.name] = enum

        for command_elem in root.iterfind('commands/command'):
            function = Function(command_elem)
            self.functions[function.name] = function

        extensions = []
        for feature_elem in root.iterfind('feature'):
            if self.useExtension(feature_elem):
                extensions.append(Extension(feature_elem, self))
        for extension_elem in root.iterfind('extensions/extension'):
            if self.useExtension(extension_elem):
                extensions.append(Extension(extension_elem, self))
        extensions.sort(key = Extension.key) # Sort by key, to insert into OrderedDict
        for extension in extensions:
            self.extensions[extension.name] = extension

        # Prune enums and functions that don't apply in this API/profile
        self.enums = {x[0]: x[1] for x in self.enums.items() if x[1].extensions}
        self.functions = {x[0]: x[1] for x in self.functions.items() if x[1].extensions}

    def useExtension(self, extension_elem):
        if extension_elem.tag == 'feature':
            return self.api == extension_elem.get('api')
        else:
            supported_re = re.compile('(?:' + extension_elem.get('supported') + ')$')
            # TODO: needs to consider ES1 required vs extensions
            return supported_re.match(self.default_extensions)

    def matchRequire(self, elem):
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

def do_alias(options, apis):
    for api in apis:
        for function in api.functions.values():
            aliases = function.elem.findall('alias')
            for a in aliases:
                print('ALIAS {} {}'.format(function.name, a.get('name')))

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
            fields.append('BUGLE_API_EXTENSION_BLOCK_GL') # TODO: get from api
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
            extension = 'BUGLE_' + function.extensions[0].name
        print('    {{ {0} }},  /* {1} */'.format(extension, function_name))
    print('};')

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

def main():
    parser = OptionParser()
    parser.add_option('--header', dest = 'header', metavar = 'FILE',
                      help = 'header file with function enumeration')
    parser.add_option('-m', '--mode', dest = 'mode', metavar = 'MODE',
                      type = 'choice', choices = ['c', 'header', 'alias'],
                      help = 'output to generate')
    parser.set_usage('Usage: %prog [options] -m mode gl.xml')
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

    apis = [GLAPI(args[0])]
    modes[options.mode](options, apis)

if __name__ == '__main__':
    main()
