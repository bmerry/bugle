#!/usr/bin/env python
from SCons.Environment import Environment
import SCons.Util

class CrossEnvironment(Environment):
    '''
    Environment that supports cross-compilation with the GCC toolchain.
    If the construction variable HOST is set, it is used as a prefix
    to search for toolchain binaries.

    This environment also overrides the SharedLibrary method to do a
    better job with regards to sonames, version numbering and symbol
    visibility.
    '''

    _cross_tools = ['cc', 'gcc', 'g++', 'as', 'gas', 'ranlib', 'ar']

    def Detect(self, progs):
        """Return the first available program in progs.

        In this version, we first try to find a cross-tool, then fall back
        to looking for a native tool
        """

        if self.has_key('HOST'):
            if not SCons.Util.is_List(progs):
                progs = [ progs ]
            if progs[0] in self._cross_tools:
                for prog in progs:
                    cross_prog = self['HOST'] + '-' + prog
                    path = self.WhereIs(cross_prog)
                    if path: return cross_prog
                # If we get here, we failed to find a cross-tool above
                print 'WARNING: did not find %s-%s, falling back' % (self['HOST'], progs[0])

        return Environment.Detect(self, progs)

    def SharedLibrary2(self, target, source, **kw):
        '''
        Overrides the default SharedLibrary builder to support
          - sonames: automatically set
          - C{version=1.2.3} creates the library with the version appended,
            and uses the leading part of the version in the SONAME
          - C{symbols=...} can be a list, a regex, or a callback (not yet
            implemented)
        Other options are passed to the original rule
        '''

        if self['PLATFORM'] == 'posix' and 'gcc' in self['TOOLS']:
            # Assume posix is the same as ELF
            if 'version' in kw:
                version = kw['version']
                version_parts = version.split('.')
                base = self.subst('${SHLIBPREFIX}' + target + '${SHLIBSUFFIX}')
                full = base + '.' + version
                soname = base + '.' + version_parts[0]

                shl_list = self.SharedLibrary(full, source,
                    SHLIBPREFIX = '', SHLIBSUFFIX = '',
                    LINKFLAGS = '-Wl,-soname,' + soname, **kw)
                self.Command(soname, shl_list[0], 'ln -s ${SOURCE.file} $TARGET')
                return self.Command(base, shl_list[0], 'ln -s ${SOURCE.file} $TARGET')

        return self.SharedLibrary(target, source, **kw)
