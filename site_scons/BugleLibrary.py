#!/usr/bin/env python

import os
import os.path
import SCons.Action

"""
An enhanced version of the SCons SharedLibrary builder, that handles
- adding sonames
- creation of symlinks
- installation of the relevant pieces to the relevant locations
"""

def copyFunc(dest, source, env):
    """
    Wrapper around SCons' copyFunc that handles symlinks correctly
    """
    if os.path.islink(source):
        linkto = os.readlink(source)
        os.symlink(linkto, dest)
        return 0
    else:
        return env['OLDINSTALL'](dest, source, env)

def symlinkFunc(target, source, env):
    os.symlink(os.path.split(source[0].path)[-1], target[0].path)
    return 0

def SharedLibrary(env, binfmt, target, source, bindir = None, libdir = None, version = None, **kw):
    """
    An enhanced version of the SCons SharedLibrary builder, that handles
      - adding sonames
      - creation of symlinks
      - installation of the relevant pieces to the relevant locations

    @param env The environment in which to build the library
    @param binfmt Either C{elf} or C{pe} to indicate the target binary format
    @param target Target name (not a list), without prefix or suffix
    @param bindir Directory to install files for C{PATH}, or C{None} to skip installation
    @param libdir Directory to install files for C{LIBPATH}, or C{None} to skip installation
    @param version String of the form a.b.c, or C{None} to avoid versioning
    """

    symlink_action = SCons.Action.Action(symlinkFunc, "SYMLINK $SOURCE $TARGET")

    if binfmt == 'elf':
        if version is not None:
            if 'gcc' in env['TOOLS']:
                version_parts = version.split('.')
                base = env.subst('${SHLIBPREFIX}' + target + '${SHLIBSUFFIX}')
                full = base + '.' + version
                soname = base + '.' + version_parts[0]

                shl_list = env.SharedLibrary(full, source,
                    SHLIBPREFIX = '', SHLIBSUFFIX = '',
                    LINKFLAGS = ['-Wl,-soname,' + soname] + env['LINKFLAGS'], **kw)
                soname_target = env.Command(soname, shl_list[0], symlink_action)
                base_target = env.Command(base, shl_list[0], symlink_action)

                if libdir is not None:
                    install_env = env.Clone(OLDINSTALL = env['INSTALL'], INSTALL = copyFunc)
                    install_env.Install(target = libdir, source = shl_list + soname_target + base_target)
                return base_target
            else:
                raise NotImplemented, 'Do not know how to set SONAME without GCC'
        else:
            shl_list = env.SharedLibrary(target, source, **kw)
            if libdir is not None:
                install_env = env.Clone(OLDINSTALL = env['INSTALL'], INSTALL = copyFunc)
                install_env.Install(target = libdir, source = shl_list)
                install_envenv.Alias('install', libdir)
    elif binfmt == 'pe':
        if 'msvc' in env['TOOLS']:
            # Builder returns [dll, lib, exp], we only need the lib for linking
            # other libraries against
            dll, lib, exp = env.SharedLibrary(target, source, **kw)

            # No symlinks required, so no need to replace the env
            if libdir is not None:
                env.Install(target = libdir, source = [lib, exp])
                env.Alias('install', libdir)
            if bindir is not None:
                env.Install(target = bindir, source = dll)
                env.Alias('install', bindir)
            return [lib]
        else:
            raise NotImplemented, 'Do not know how to build shared libraries for PE without MSVC'
            # TODO what does mingw output?
    else:
        raise NotImplemented, 'Do not know how to build shared libraries for ' + binfmt
