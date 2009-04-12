#!/usr/bin/env python
from SCons import *
from subprocess import *
import re

def find_header(env, name):
    # TODO: -I should come from CPPPATH
    sp = Popen([env['CC'], '-E', '-I.', '-I' + env['topbuilddir'].path, '-'],
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

def alias_emitter(target, source, env):
    # Make sure the generator is treated as a source for dependency purposes
    source.append('#source/gengl/gengl.perl')
    return target, source

def alias_generator(source, target, env, for_signature):
    cmd = 'perl ' + source[-1].path + ' --mode=alias'
    for header in source[0:-1]:
        cmd += ' ' + header.path
    cmd += ' > ' + target[0].path
    return cmd

def generate(env, **kw):
    alias_builder = env.Builder(
            generator = alias_generator,
            emitter = alias_emitter)
    env.Append(
            BUILDERS = {'BudgieAlias': alias_builder},
            find_header = find_header)

def exists(env):
    return 1
