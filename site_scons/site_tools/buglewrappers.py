#!/usr/bin/env python

import os
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

def _build_shared_library(env, binfmt, target, source, bindir = None, libdir = None, version = None, **kw):
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
                    install_env.Alias('install', libdir)
                return base_target
            else:
                raise NotImplemented, 'Do not know how to set SONAME without GCC'
        else:
            shl_list = env.SharedLibrary(target, source, **kw)
            if libdir is not None:
                install_env = env.Clone(OLDINSTALL = env['INSTALL'], INSTALL = copyFunc)
                install_env.Install(target = libdir, source = shl_list)
                install_env.Alias('install', libdir)
            return shl_list
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
        elif 'mingw' in env['TOOLS']:
            dll = env.SharedLibrary(target, source, **kw)
            if bindir is not None:
                env.Install(target = bindir, source = dll)
                env.Alias('install', bindir)
            return [dll]
        else:
            raise NotImplemented, 'Do not know how to build shared libraries for PE without MSVC'
            # TODO what does mingw output?
    else:
        raise NotImplemented, 'Do not know how to build shared libraries for ' + binfmt

def _c_cxx_file(env, func, source, package_sources, **kw):
    '''
    Returns a target for a .y or .l file.
    If the corresponding C file already exists in the srcdir, it is used
    directly rather than generating a rule. The C and header files are also
    scheduled for packaging.

    @param package_sources A list to be extended with nodes to package
    @return A Node for the .c file
    '''

    if package_sources is None:
        package_sources = []
    source = env.arg2nodes(source, env.fs.Entry)
    all_targets = []
    for s in source:
        basename, ext = os.path.splitext(s.srcnode().abspath)
        srcdir = s.dir.srcnode().path
        if ext == '.yy' or ext == '.ll':
            target_ext = env['CXXFILESUFFIX']
        else:
            target_ext = env['CFILESUFFIX']
        if os.path.exists(basename + target_ext):
            targets = [env.File(basename + target_ext)]
            if ext == '.y' or ext == '.yy':
                targets.append(env.File(basename + '.h'))
            package_sources.extend(targets)
        else:
            targets = func(s, **kw)
            for t in targets:
                out = env['PACKAGEROOT'].File(srcdir + '/' + t.name)
                package_sources.extend(env.CopyTo(out, t))
        all_targets.append(targets[0])
    return all_targets

def _cfile(env, source, package_sources = None, **kw):
    return _c_cxx_file(env, env.CFile, source, package_sources, **kw)

def _cxxfile(env, source, package_sources = None, **kw):
    return _c_cxx_file(env, env.CXXFile, source, package_sources, **kw)

def generate(env, **kw):
    env.AddMethod(_build_shared_library, 'BugleSharedLibrary')
    env.AddMethod(_cfile, 'BugleCFile')
    env.AddMethod(_cxxfile, 'BugleCXXFile')

def exists(env):
    return 1
