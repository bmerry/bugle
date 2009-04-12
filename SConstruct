#!/usr/bin/env python
env = Environment(
        CPPPATH = ['#/include', '#'],
        CXXFLAGS = '-Wall -g',
        YACCHXXFILESUFFIX = '.h',
        BUDGIEPATH = '.',
        srcdir = Dir('#').srcnode(),
        builddir = Dir('#')
        )
Export('env')

SConscript('budgie/SConscript')
SConscript('src/SConscript')
