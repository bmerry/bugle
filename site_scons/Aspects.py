#!/usr/bin/env python

import SCons

class Aspect:
    def __init__(self, group, name, help, choices = None, default = None, metavar = None, multiple = False):
        if choices is None and multiple:
            raise ValueError, 'multiple aspect can only be used with choices'

        self.group = group
        self.name = name
        self.help = help
        self.choices = choices
        self.multiple = multiple
        self.default = default
        self.metavar = metavar

        self.value = None
        self.known = False
        self.explicit = None    # Explicitly set value from user
        self.on_stack = False

    def _convert(self, value):
        if self.multiple:
            if value == 'all':
                return self.choices
            else:
                return set(value.split(','))
        else:
            return value

    def _set(self, value, explicit):
        value = self._convert(value)
        if self.multiple:
            for i in value:
                if i not in self.choices:
                    raise ValueError, "%s is not a valid value for %s" % (i, self.name)
        elif self.choices is not None:
            if value not in self.choices:
                raise ValueError, "%s is not a valid value for %s" % (value, self.name)
        self.value = value
        self.known = True
        self.explicit = explicit

    def set(self, value):
        self._set(value, value)

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
            self._set(text, None)
        return self.value

    def __str__(self):
        if self.multiple:
            value = self.get()
            if self.get() == self.choices:
                return 'all'
            else:
                return ','.join(self.get())
        else:
            return str(self.get())

    def is_default(self):
        if callable(self.default):
            default = self._convert(self.default(self))
        else:
            default = self._convert(self.default)
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
        """
        Prints a user-readable description of the current configuration
        """
        print
        print "*** Configuration ***"
        print
        for a in self.ordered:
            if a.get() is None:
                continue;
            value = str(a)
            if value == '':
                value = '<empty>'
            print "%-20s = %-20s" % (a.name, value),
            tags = []
            if a.explicit is not None:
                tags.append('from user')
            elif callable(a.default):
                tags.append('derived')
            else:
                tags.append('default')
            if a.explicit is not None and a.explicit != value:
                tags.append('modified')
            print '(' + ', '.join(tags) + ')'
        print
        print "***"
        print

    def Help(self):
        '''
        Returns a help string describing the options
        '''
        help = ''
        for a in self.ordered:
            values = '<string>'
            if a.choices is not None:
                values = '|'.join(a.choices)
                if a.multiple:
                    values = '(' + values + ')+ | all'
            help += '%s=%s\n            %s\n\n' % (a.name, values, a.help)
        return help

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
            if aspect.explicit is not None:
                explicit[aspect.name] = aspect.explicit
        f = file(filename, 'w')
        try:
            print >>f, repr(explicit)
        finally:
            f.close()
