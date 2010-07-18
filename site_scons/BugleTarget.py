#!/usr/bin/env python

import SCons

class Target(dict):
    '''
    Contains the keywords that will be passed to a builder. This allows
    the sources, libraries etc, to be built up from different places
    '''
    def __init__(self):
        self['source'] = []
        self['target'] = []
        self['LIBS'] = ['$LIBS']    # ensures we add to rather than replace


