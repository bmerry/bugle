#!/usr/bin/env python
from SCons import *
from subprocess import *
import re

def find_header(env, name):
    # TODO: -I should come from CPPPATH
    sp = Popen([env['CC'], '-E', '-I.', '-I' + env['srcdir'].path, '-'],
            stdin = PIPE, stdout = PIPE, stderr = PIPE)
    (out, err) = sp.communicate('#include <' + name + '>\n')
    errcode = sp.wait()
    if errcode != 0:
        raise NameError, err.rstrip("\r\n")

    s = re.compile(r'^# \d+ "(.*' + name + r')"', re.M)
    matches = s.search(out)
    if matches is not None:
        return matches.group(1)

    # Preprocessor didn't find it, fall back and try some known paths
    file = env.FindFile(name, [
        '/usr/include',
        '/usr/X11R6/include',
        '/usr/local/include',
        '/usr/include/w32api'])
    if file is not None:
        return file

    raise NameError, 'Could not find ' + name

def gengl_emitter(target, source, env):
    '''Make the generator a dependency'''
    env.Depends(target, env['GENGL'])
    return target, source

def generate(env, **kw):
    alias_builder = env.Builder(
            action = Action.Action('perl $GENGL --mode=alias $SOURCES > $TARGET', '$ALIASCOMSTR'),
            emitter = gengl_emitter)
    apitables_c_builder = env.Builder(
            action = Action.Action('perl $GENGL --mode=c --header=$SOURCES > $TARGET', '$APITABLESCCOMSTR'),
            emitter = gengl_emitter)
    apitables_h_builder = env.Builder(
            action = Action.Action('perl $GENGL --mode=header --header=$SOURCES > $TARGET', '$APITABLESHCOMSTR'),
            emitter = gengl_emitter)
    env.Append(
            BUILDERS = {
                'BudgieAlias': alias_builder,
                'ApitablesC': apitables_c_builder,
                'ApitablesH': apitables_h_builder
                },
            GENGL = env['srcdir'].File('gengl/gengl.perl'),
            find_header = find_header)

def exists(env):
    return 1
