#!/usr/bin/env python

'''
Registers a builder to produce files from budgie. The builder takes
exactly 5 targets and 2 sources.
TODO: say what they are
The environment must also contain BUDGIE (node of the executable) and
BCPATH (include directories to scan)
'''

from SCons import *
import re

def bc_scanner_function(node, env, path, arg = None):
    '''Searches a .bc file for INCLUDE lines'''
    contents = node.get_contents()
    matches = re.findall(r'^\s*INCLUDE\s*(\S+)\s*$', contents, re.M)
    deps = []
    for x in matches:
        dep = env.FindFile(x, path)
        if dep is None:
            raise NameError, "Could not find " + x
        else:
            deps.append(dep)
    return deps

def bc_emitter(target, source, env):
    '''Make budgie itself a dependency'''
    env.Depends(target, env['BUDGIE'])
    return target, source

def generate(env, **kw):
    bc_scanner = env.Scanner(
            function = bc_scanner_function,
            path_function = Scanner.FindPathDirs('BCPATH'),
            recursive = 1)

    bc_builder = env.Builder(
            action = Action.Action('$BUDGIECOM', '$BUDGIECOMSTR'),
            emitter = bc_emitter,
            source_suffix = '.bc',
            source_scanner = Tool.SourceFileScanner)

    env.Append(
            BUILDERS = {'Budgie': bc_builder},
            BUDGIECOM = '${BUDGIE.path} $_BCINCPATH -c ${TARGETS[0].path} -2 ${TARGETS[1].path} -d ${TARGETS[2].path} -t ${TARGETS[3].path} -l ${TARGETS[4].path} -T ${SOURCES[0].path} ${SOURCES[1].abspath}',
            _BCINCPATH = '$( ${ _concat("-I ", BCPATH, "", __env__, RDirs)} $)',
            )
    Tool.SourceFileScanner.add_scanner('.bc', bc_scanner)

def exists(env):
    return 1
