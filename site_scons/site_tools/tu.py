#!/usr/bin/env python
from SCons import *
import SCons.Util
import os.path

def move_tu(target, source, env):
    '''
    Finds the .tu file generated by GCC and renames it to match the target.
    '''
    source_name = os.path.split(str(source[0]))[-1]
    for ext in ['.tu', '.t00.tu', '.001t.tu']:
        tu_name = source_name + ext
        exists = False
        try:
            f = file(tu_name, 'r')
            f.close()
            exists = True
        except IOError:
            exists = False
        if exists:
            if str(target) != tu_name:
                return env.Execute(Script.Move(target[-1], tu_name))
    print ".tu file could not be found"
    return 1

def generate(env):
    env['TUCOM'] = '$CCCOM -fdump-translation-unit'

    tu_builder = env.Builder(
            action = [
                Action.Action('$TUCOM', '$TUCOMSTR'),
                Action.Action(move_tu, None)
                ],
            source_suffix = '.c',
            target_suffix = '.o')

    # CCache doesn't understand the -fdump-translation-unit option, so disable it
    env.Append(BUILDERS = {'Tu': tu_builder}, ENV = {'CCACHE_DISABLE': '1'})

def exists(env):
    if 'gcc' not in env['TOOLS']:
        print "Warning: GCC not found. Did you list gcc before tu in the tools?"
        return False
    v = env['CCVERSION'].split('.')
    if (v[0] == '4' and v[1] == '0') or (v[0] == '3' and v[1] == '0') or (v[0] < 3):
        print "Warning: GCC %s does not support -fdump-translation-unit" % env['CCVERSION']
        return False
    return True