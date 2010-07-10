#!/usr/bin/env python

import os
import SCons.Action

"""
An enhanced version of the SCons SharedLibrary builder, that handles
- adding sonames
- creation of symlinks
- installation of the relevant pieces to the relevant locations
"""

def _relpath(path, start):
    """
    Same idea as os.path.relpath, but without requiring Python 2.6.
    """
    src = start.split(os.path.sep)
    trg = path.split(os.path.sep)
    while src and trg and src[0] == trg[0]:
        del src[0:1]
        del trg[0:1]
    parts = ['..' for x in src] + trg
    return '/'.join(parts)

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
            outputs = env.SharedLibrary(target, source, **kw)
            if bindir is not None:
                env.Install(target = bindir, source = outputs[0])
                env.Alias('install', bindir)
            return [outputs[0]]
        else:
            raise NotImplemented, 'Do not know how to build shared libraries for PE without MSVC'
    else:
        raise NotImplemented, 'Do not know how to build shared libraries for ' + binfmt

def _c_cxx_file(env, builder, source, srcdir, package_sources, **kw):
    '''
    Returns a target for a .y or .l file.
    If the corresponding C file already exists in the srcdir, it is used
    directly rather than generating a rule. The C and header files are also
    scheduled for packaging.

    @param package_sources A list to be extended with nodes to package
    @return A Node for the .c file
    '''

    source = env.arg2nodes(source, env.fs.Entry)
    all_targets = []
    for s in source:
        basename, ext = os.path.splitext(_relpath(s.srcnode().path, srcdir.path))
        if ext == '.yy' or ext == '.ll':
            target_ext = env['CXXFILESUFFIX']
        else:
            target_ext = env['CFILESUFFIX']
        target = [basename + target_ext]
        if ext == '.y' or ext == '.yy':
            target.append(basename + '.h')

        target = env.BuglePrebuild(
                target = target,
                source = [s],
                srcdir = srcdir,
                builder = builder,
                package_sources = package_sources,
                implicit_target = True,
                **kw)
        all_targets.append(target[0])   # Only return the .c node
    return all_targets

def _cfile(env, source, srcdir, package_sources = None, **kw):
    return _c_cxx_file(env, env.CFile, source, srcdir, package_sources, **kw)

def _cxxfile(env, source, srcdir, package_sources = None, **kw):
    return _c_cxx_file(env, env.CXXFile, source, srcdir, package_sources, **kw)

def _prebuild_wrapper(env, target, source, srcdir, builder, package_sources = None,
                      implicit_target = False, dir = None, **kw):
    '''
    Wraps another builder, but if all the targets already exist in srcdir
    then they are just copied to the target. Additionally, the targets
    are marked for packaging

    @param target: List of relative filenames to be built
    @type target: List of strings
    @param source: List of source nodes
    @type source: List of nodes and string objects (like any builder)
    @param srcdir: Directory relative to which targets are searched
    @type srcdir: Node object
    @param builder: The underlying builder to use
    @type builder
    @param package_sources: A list to be extended with nodes to package
    @type package_sources: mutable sequence
    @param implicit_target: If true, the target parameter is not given to the builder
    @type implicit_target: boolean
    @param dir: If set, specifies a whole directory to transfer
    @type dir: str
    @return A list of nodes corresponding to targets
    '''
    if package_sources is None:
        package_sources = []
    source = env.arg2nodes(source, env.fs.Entry)
    target = env.Flatten(target)
    have_as_source = True

    for t in target:
        srcpath = os.path.join(srcdir.abspath, t)
        if not os.path.exists(srcpath):
            have_as_source = False
            break

    dir_node = None
    dir_src_node = None
    if dir is not None:
        dir_node = env.Dir(dir)
        dir_src_node = srcdir.Dir(dir)

    if have_as_source:
        if dir is None:
            target = env.arg2nodes(target, env.fs.Entry)
            target = [env.CopyAs(t, t.srcnode()) for t in target]
        else:
            target = [env.CopyAs(dir_node, dir_src_node)]
    elif implicit_target:
        target = builder(source, **kw)
    else:
        target = builder(target, source, **kw)

    out = env['PACKAGEROOT'].Dir(srcdir.path)
    if dir is None:
        for t in target:
            package_sources.extend(env.CopyTo(out, t))
    else:
        package_sources.extend(env.CopyAs(out.Dir(dir), dir_node))
        env.Clean(target, dir_node)
    return target

def generate(env, **kw):
    env.AddMethod(_build_shared_library, 'BugleSharedLibrary')
    env.AddMethod(_cfile, 'BugleCFile')
    env.AddMethod(_cxxfile, 'BugleCXXFile')
    env.AddMethod(_prebuild_wrapper, 'BuglePrebuild')

def exists(env):
    return 1
