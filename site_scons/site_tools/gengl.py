#!/usr/bin/env python
from SCons import *

def alias_emitter(target, source, env):
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
    env.Append(BUILDERS = {'BudgieAlias': alias_builder})

def exists(env):
    return 1
