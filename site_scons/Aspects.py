#!/usr/bin/env python

import SCons

class Aspect:
    def __init__(self, name, help, choices = None, default = None, metavar = None):
        self.name = name
        self.help = help
        self.choices = choices
        self.default = default
        self.metavar = metavar

        self.value = None
        self.known = False
        self.explicit = False
        self.on_stack = False

    def _set(self, value, explicit):
        if self.choices is not None and value not in self.choices:
            raise RuntimeError, "%s is not a valid value for %s" % (value, self.name)
        self.value = value
        self.known = True
        self.explicit = explicit

    def set(self, value):
        self._set(value, True)

    def get(self):
        if not self.known:
            if self.on_stack:
                raise RuntimeError, "Cyclic dependency computing " + self.name
            value = None
            if callable(self.default):
                self.on_stack = True
                text = self.default(self)
                self.on_stack = False
            else:
                text = self.default
            self._set(text, False)
        return self.value

class AspectParser:
    def __init__(self):
        self.aspects = {}

    def AddAspect(self, aspect):
        self.aspects[aspect.name] = aspect

    def Resolve(self):
        # Force callable defaults to run before postprocessing
        for a in self.aspects.values():
            a.get()

    def __setitem__(self, key, value):
        if key not in self.aspects:
            raise RuntimeError, key + ' is not a valid option'
        self.aspects[key].set(value)

    def __getitem__(self, key):
        if key not in self.aspects:
            raise RuntimeError, key + ' is not a valid option'
        return self.aspects[key].get()

    def __contains__(self, key):
        return key in self.aspects

    def Report(self):
        for name in sorted(self.aspects.keys()):
            a = self.aspects[name]
            print name, '=', str(a.get()),
            if a.explicit:
                print '(from user)'
            elif callable(a.default):
                print '(computed)'
            else:
                print '(default)'

    def Help(self):
        # TODO fill in
        pass
