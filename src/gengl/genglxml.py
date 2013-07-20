#!/usr/bin/env python3

"""
Generate header and C files for BuGLe from the OpenGL extension registry.

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

"""

import sys
import os.path
import re
from optparse import OptionParser
from textwrap import dedent

apis_path = os.path.abspath(__file__)
apis_path = os.path.dirname(os.path.dirname(os.path.dirname(apis_path)))
apis_path = os.path.join(apis_path, 'khronos-api')
sys.path.insert(0, apis_path)
import reg
import genglxmltables

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

def feature_key(feature):
    '''
    Generate a tuple which can be used to sort extensions. Extensions
    are sorted into the order:
     - versions, by decreasing version number
     - Khronos-approved extensions (ARB/KHR/OES)
     - multi-vendor extensions (EXT)
     - vendor extensions
     - extension groups
    '''
    if (feature.elem.tag == 'feature'):
        return (0, -float(feature.number))
    elif (feature.category == 'ARB' or
          feature.category == 'KHR' or
          feature.category == 'OES'):
        return (1, feature.name)
    elif (feature.category == 'EXT'):
        return (2, feature.name)
    else:
        return (3, feature.name)

def sort_and_save(generator):
    def run(features):
        reg.regSortFeatures(features)
        generator.features = sorted(features, key = feature_key)
    return run

class NullOutputGenerator:
    """
    Provides the functions expected by the registry class, with empty
    implementations.
    """
    def __init__(
            self,
            errFile = sys.stderr,
            warnFile = sys.stderr,
            diagFile = sys.stdout):
        pass

    def beginFile(self, genOpts):
        pass

    def endFile(self):
        pass

    def beginFeature(self, interface, emit):
        pass

    def endFeature(self):
        pass

    def genType(self, typeinfo, name):
        pass

    def genEnum(self, enuminfo, name):
        pass

    def genCmd(self, cmdinfo, name):
        pass

    def logMsg(level, *args):
        pass

class AliasOutputGenerator(NullOutputGenerator):
    def genCmd(self, cmdinfo, name):
        name = cmdinfo.elem.find('proto/name').text
        aliases = cmdinfo.elem.findall('alias')
        for a in aliases:
            print('ALIAS {} {}'.format(name, a.get('name')))

class TableOutputGenerator(NullOutputGenerator):
    def __init__(
            self,
            errFile = sys.stderr,
            warnFile = sys.stderr,
            diagFile = sys.stdout):
        self.cmd_features = {}
        self.cur_feature = None

    def beginFeature(self, interface, emit):
        if emit:
            self.cur_feature = interface

    def endFeature(self):
        self.cur_feature = None

    def genCmd(self, cmdinfo, name):
        # TODO: this approach assumes only one feature per command.
        # This can't be avoided using a generator, because it only
        # emits each command once.
        if self.cur_feature is not None:
            self.cmd_features[name] = self.cur_feature.get('name')

def run(options, args, generator):
    r = reg.Registry()
    for fname in args:
        r.loadFile(fname)
        genOpts = reg.CGeneratorOptions(
            filename          = 'GL/glext.h',
            apiname           = 'gl',
            profile           = 'compatibility',
            defaultExtensions = 'gl',
            sortProcedure     = sort_and_save(generator))
        r.setGenerator(generator)
    r.apiGen(genOpts)

def do_alias(options, args):
    run(options, args, AliasOutputGenerator())

def do_header(options, args):
    gen = NullOutputGenerator()
    run(options, args, gen)

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
    for feature in gen.features:
        if feature.emit:
            print("#define BUGLE_{0} {1}".format(feature.name, index))
            index += 1
    print("#define BUGLE_API_EXTENSION_COUNT {0}".format(index))
    print(dedent("""
        extern const bugle_api_extension_data _bugle_api_extension_table[BUGLE_API_EXTENSION_COUNT];
        extern const bugle_api_function_data _bugle_api_function_table[];
        extern const bugle_api_enum_data * const _bugle_api_enum_table[];
        extern const int _bugle_api_enum_count[];
        #endif /* BUGLE_SRC_APITABLES_H */"""))

def feature_descendants(feature_map, feature):
    ans = [feature]
    q = [feature]
    while q:
        cur = q.pop()
        children = genglxmltables.extension_children.get(cur.name, [])
        for c in children:
            if c in feature_map:
                f = feature_map[c]
                if f not in ans:
                    ans.append(f)
                    q.append(f)
    return ans

def do_c(options, args):
    gen = TableOutputGenerator()
    run(options, args, gen)

    print('/* Generated by ' + sys.argv[0] + '. Do not edit. */')
    print(dedent('''
        #if HAVE_CONFIG_H
        # include <config.h>
        #endif
        #include <bugle/apireflect.h>
        #include "apitables.h"
        '''))
    feature_map = {x.name: x for x in gen.features}
    for feature in gen.features:
        if feature.emit:
            desc = feature_descendants(feature_map, feature)
            print(dedent('''
                static const bugle_api_extension extension_group_{0}[] =
                {{''').format(feature.name))
            for d in sorted(desc, key = feature_key):
                print('    BUGLE_{0},'.format(d.name))
            print('    NULL_EXTENSION')
            print('};')

    print(dedent('''
        const bugle_api_extension_data _bugle_api_extension_table[] =
        {'''))
    for feature in gen.features:
        fields = []
        if feature.elem.tag == 'feature':
            fields.append('"{}"'.format(feature.number))
        else:
            fields.append('NULL')
        fields.append('"{}"'.format(feature.name))
        fields.append('extension_group_{}'.format(feature.name))
        fields.append('BUGLE_API_EXTENSION_BLOCK_GL') # TODO
        print('    {{ {0} }},'.format(', '.join(fields)))
    print('};')

    functions = load_function_ids(options.header)
    print(dedent('''
        const bugle_api_function_data _bugle_api_function_table[] =
        {'''))
    for function in functions:
        if function not in gen.cmd_features:
            print("WARNING: Function {} not found in registry".format(function), file = sys.stderr)
            feature = 'NULL_EXTENSION'
        else:
            feature = 'BUGLE_' + gen.cmd_features[function]
        print('    {{ {0} }},  /* {1} */'.format(feature, function))
    print('};')

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
    modes[options.mode](options, args)

if __name__ == '__main__':
    main()
