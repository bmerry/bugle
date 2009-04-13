#!/usr/bin/env python
Import('srcdir', 'builddir', 'env')

SConscript('budgie/SConscript')
SConscript('src/SConscript')
SConscript('filters/SConscript')

env.AlwaysBuild(env.Command('lib/libgnu.la', [], 'make -C lib'))

budgie_outputs = [
        'include/budgie/call.h',
        'include/budgie/types2.h',
        'budgielib/defines.h',
        'budgielib/tables.c',
        'budgielib/lib.c'
        ]
env.Budgie(budgie_outputs, ['src/data/gl.tu', 'bc/gl-glx.bc'])

headers = ['GL/gl.h', 'GL/glext.h', 'GL/glx.h', 'GL/glxext.h']
headers = [env['find_header'](env, h) for h in headers]
# TODO change to bc/alias.bc
env.BudgieAlias(target = ['bc/gl/alias.bc'], source = headers)
env.ApitablesC(target = ['src/apitables.c'], source = ['budgielib/defines.h'] + headers)
env.ApitablesH(target = ['src/apitables.h'], source = ['budgielib/defines.h'] + headers)
