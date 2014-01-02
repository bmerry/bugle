#!/usr/bin/env python
from SCons import *
from subprocess import *
import re
import tempfile
import os

def gengl_emitter(target, source, env):
    '''Make the generator a dependency'''
    env.Depends(target, env['GENGL'])
    env.Depends(target, env['GENGLTABLES'])
    return target, source

def generate(env, **kw):
    alias_builder = env.Builder(
            action = Action.Action('python $GENGL --mode=alias --gltype=$GLTYPE $SOURCES > $TARGET', '$ALIASCOMSTR'),
            emitter = gengl_emitter)
    apitables_c_builder = env.Builder(
            action = Action.Action('python $GENGL --mode=c --gltype=$GLTYPE --header=$SOURCES > $TARGET', '$APITABLESCCOMSTR'),
            emitter = gengl_emitter)
    apitables_h_builder = env.Builder(
            action = Action.Action('python $GENGL --mode=header --gltype=$GLTYPE --header=$SOURCES > $TARGET', '$APITABLESHCOMSTR'),
            emitter = gengl_emitter)
    env.Append(
            BUILDERS = {
                'BudgieAlias': alias_builder,
                'ApitablesC': apitables_c_builder,
                'ApitablesH': apitables_h_builder
                },
            GENGL = env.File('#src/gengl/genglxml.py'),
            GENGLTABLES = env.File('#src/gengl/genglxmltables.py'))

def exists(env):
    return 1
