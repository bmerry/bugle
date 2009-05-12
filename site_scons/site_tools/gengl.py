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
    '''Make sure the generator is treated as a source for dependency purposes'''
    source.append(env['srcdir'].File('gengl/gengl.perl'))
    return target, source

def alias_generator(source, target, env, for_signature):
    cmd = 'perl ' + source[-1].path + ' --mode=alias'
    for header in source[0:-1]:
        cmd += ' ' + header.path
    cmd += ' > ' + target[0].path
    return Action.Action(cmd, '$ALIASCOMSTR')

def apitables_c_generator(source, target, env, for_signature):
    cmd = 'perl ' + source[-1].path + ' --mode=c --header=' + source[0].path
    for header in source[1:-1]:
        cmd += ' ' + header.path
    cmd += ' > ' + target[0].path
    return Action.Action(cmd, '$APITABLESCCOMSTR')

def apitables_h_generator(source, target, env, for_signature):
    cmd = 'perl ' + source[-1].path + ' --mode=header --header=' + source[0].path
    for header in source[1:-1]:
        cmd += ' ' + header.path
    cmd += ' > ' + target[0].path
    return Action.Action(cmd, '$APITTABLESHCOMSTR')

def generate(env, **kw):
    alias_builder = env.Builder(
            generator = alias_generator,
            emitter = gengl_emitter)
    apitables_c_builder = env.Builder(
            generator = apitables_c_generator,
            emitter = gengl_emitter)
    apitables_h_builder = env.Builder(
            generator = apitables_h_generator,
            emitter = gengl_emitter)
    env.Append(
            BUILDERS = {
                'BudgieAlias': alias_builder,
                'ApitablesC': apitables_c_builder,
                'ApitablesH': apitables_h_builder
                },
            find_header = find_header)

def exists(env):
    return 1
