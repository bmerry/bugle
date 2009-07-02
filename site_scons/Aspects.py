#!/usr/bin/env python

import SCons

class Aspect:
    def __init__(self, group, name, help, choices = None, default = None, metavar = None):
        self.group = group
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

    def is_default(self):
        if callable(self.default):
            default = self.default(self)
        else:
            default = self.default
        return default == self.get()

class AspectParser:
    def __init__(self):
        self.aspects = {}
        self.ordered = []

    def AddAspect(self, aspect):
        self.ordered.append(aspect)
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
        print
        print "*** Configuration ***"
        print
        for a in self.ordered:
            value = a.get()
            if value is None:
                continue
            if value == '':
                value = '<empty>'
            print "%-20s = %-20s" % (a.name, value),
            if a.explicit:
                print '(from user)'
            elif callable(a.default):
                print '(derived)'
            else:
                print '(default)'
        print
        print "***"
        print

    def Help(self):
        # TODO fill in
        pass

    def LoadFile(self, filename):
        try:
            f = file(filename, 'r')
            try:
                globls = {}
                values = eval(f.read(), globls)
                self.Load(values, filename)
                print 'Loaded configuration from', filename
            finally:
                f.close()
        except IOError:
            pass    # Don't care if the file doesn't exist

    def Load(self, args, source):
        for name, value in args.iteritems():
            if name in self:
                self[name] = value
            else:
                raise RuntimeError, name + ' is not a valid option (' + source + ')'

    def Save(self, filename):
        explicit = {}
        for aspect in self.aspects.values():
            if aspect.explicit:
                explicit[aspect.name] = aspect.get()
        f = file(filename, 'w')
        try:
            print >>f, repr(explicit)
        finally:
            f.close()
