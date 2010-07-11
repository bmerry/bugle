#!/usr/bin/env python
from SCons import *
from subprocess import *
import re
import tempfile
import os

def find_header(env, name):
    (fd, tmpname) = tempfile.mkstemp(suffix = '.c', text = True)
    try:
        fh = os.fdopen(fd, 'w+')
        print >>fh, '#ifdef _WIN32'
        print >>fh, '#define WIN32_LEAN_AND_MEAN'
        print >>fh, '#include <windows.h>'
        print >>fh, '#endif'
        print >>fh, '#include <' + name + '>\n'
        fh.close()
        fd = -1

        preproc_opt = '-E'
        if env['INCPREFIX'] == '/I':
            preprop_opt = '/E'
        incflags = []
        for i in env['CPPPATH']:
            incflags.append(env['INCPREFIX'] + env.Dir(i).path)
        cmd = env.Flatten([env['CC'], env['CCFLAGS'], preproc_opt, incflags, tmpname])
        # print "Running", ' '.join(cmd)
        sp = Popen(cmd, stdin = PIPE, stdout = PIPE, stderr = PIPE)
        (out, err) = sp.communicate()
        errcode = sp.wait()
        if errcode != 0:
            raise NameError, err.rstrip("\r\n")
    finally:
        if fd != -1: os.close(fd)
        os.remove(tmpname)

    s = re.compile(r'^#(?:line)? \d+ "(.*' + name + r')"', re.M)
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

def find_header_factory(env):
    def factory(name):
        path = find_header(env, name)
        return env.File(path)

    return factory

def gengl_emitter(target, source, env):
    '''Make the generator a dependency'''
    env.Depends(target, env['GENGL'])
    return target, source

def generate(env, **kw):
    alias_builder = env.Builder(
            action = Action.Action('perl $GENGL --mode=alias $SOURCES > $TARGET', '$ALIASCOMSTR'),
            emitter = gengl_emitter,
            source_factory = find_header_factory(env))
    apitables_c_builder = env.Builder(
            action = Action.Action('perl $GENGL --mode=c --header=$SOURCES > $TARGET', '$APITABLESCCOMSTR'),
            emitter = gengl_emitter,
            source_factory = find_header_factory(env))
    apitables_h_builder = env.Builder(
            action = Action.Action('perl $GENGL --mode=header --header=$SOURCES > $TARGET', '$APITABLESHCOMSTR'),
            emitter = gengl_emitter,
            source_factory = find_header_factory(env))
    env.Append(
            BUILDERS = {
                'BudgieAlias': alias_builder,
                'ApitablesC': apitables_c_builder,
                'ApitablesH': apitables_h_builder
                },
            GENGL = env.File('#src/gengl/gengl.perl'),
            find_header = find_header)

def exists(env):
    return 1
