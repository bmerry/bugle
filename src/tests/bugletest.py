#!/usr/bin/env python
from __future__ import print_function, division
import sys

if sys.hexversion < 0x02070000:
    raise RuntimeError('Python 2.7+ required')

import re
import argparse
import os
import os.path
import subprocess
import tempfile

class TestFailed(Exception):
    def __init__(self, value):
        Exception.__init__(self, value)

class TestSkipped(Exception):
    def __init__(self, value):
        Exception.__init__(self, value)

def appendpath(old, value):
    if old:
        return value + os.pathsep + old
    else:
        return value

class TestSuite(object):
    def __init__(self, suite):
        self.suite = suite

    def run(self, args):
        '''Run the test, and throw exceptions on failure or skip.'''
        raise NotImplementedError()

class SimpleSuite(TestSuite):
    '''Runs the suite, optionally loading bugle with a chain'''

    def __init__(self, suite, chain = None):
        TestSuite.__init__(self, suite)
        self.chain = chain

    def envars(self, args):
        newenv = dict()
        if self.chain is not None:
            if args.library_path:
                if os.name == 'posix':
                    newenv['LD_LIBRARY_PATH'] = appendpath(os.environ.get('LD_LIBRARY_PATH'), args.library_path)
                    newenv['LD_PRELOAD'] = 'libbugle.so'
                else:
                    newenv['PATH'] = appendpath(os.environ.get('PATH'), args.library_path)
            if args.filter_dir:
                newenv['BUGLE_FILTER_DIR'] = args.filter_dir
            if args.filters:
                newenv['BUGLE_FILTERS'] = args.filters
            newenv['BUGLE_CHAIN'] = self.chain
        return newenv

    def run_gdb(self, args, newenv, logfile = None):
        prog = ['gdb']
        for (key, value) in newenv.items():
            prog.extend(['-ex', 'set env {}={}'.format(key, value)])
        prog.extend([
            '--args', args.tester,
            '--suite', self.suite])
        if logfile is not None:
            prog.extend(['--log', logfile])
        print(prog)
        os.execvp(prog[0], prog)
        # Should never be reached
        raise TestFailed('GDB did not run')

    def run(self, args):
        newenv = self.envars(args)
        if args.gdb:
            self.run_gdb(args, newenv)
        env = dict(os.environ)
        env.update(newenv)
        errcode = subprocess.call([args.tester, '--suite', self.suite], env = env)
        if errcode != 0:
            raise TestFailed('exit code ' + str(errcode))

class LogSuite(SimpleSuite):
    def __init__(self, suite, chain):
        SimpleSuite.__init__(self, suite, chain)

    def run(self, args):
        (handle, logfile) = tempfile.mkstemp(dir = '.', prefix = self.suite, suffix = '.log', text = True)
        reflog = None
        orig_lines = []
        try:
            os.close(handle)

            newenv = self.envars(args)

            if args.gdb:
                self.run_gdb(args, newenv, logfile)

            env = dict(os.environ)
            env.update(newenv)
            sp = subprocess.Popen([args.tester, '--suite', self.suite, '--log', logfile],
                    stdout = subprocess.PIPE, env = env)
            (out, err) = sp.communicate()
            errcode = sp.wait()
            if errcode != 0:
                raise TestFailed('exit code ' + str(errcode))

            orig_lines = out.decode('utf-8').splitlines()
            if len(orig_lines) == 0:
                raise TestFailed('no standard output')
            # Some filters output logging on shutdown, AFTER the status is
            # written, so look for status anywhere
            status = None
            lines = []
            for line in orig_lines:
                if line == 'FAILED':
                    raise TestFailed('')
                elif line in ('SKIPPED', 'RAN'):
                    status = line
                else:
                    lines.append(line)
            if status is None:
                raise TestFailed('Did not find any status lines')
            if not 'RAN' in orig_lines:
                raise TestSkipped('')

            reflog = file(logfile, 'r')
            reflines = reflog.readlines()
            reflines = ['^' + x.rstrip('\r\n') + '$' for x in reflines]
            if len(reflines) == 0 and 'RAN' in lines:
                raise TestFailed('no reference output')
            # There will be some startup stuff not in the log
            while len(lines) > 0 and not re.match(reflines[0], lines[0]):
                del lines[0]
            if len(lines) == 0:
                raise TestFailed('did not match ' + reflines[0] + ' at line 1\n' + '\n'.join(orig_lines))
            if len(lines) < len(reflines):
                raise TestFailed('not enough output')
            for i in range(len(reflines)):
                if not re.match(reflines[i], lines[i]):
                    raise TestFailed("`" + lines[i] + "' did not match `" + reflines[i] + "' at line " + str(i + 1) + '\n' + '\n'.join(orig_lines))
        finally:
            if reflog is not None:
                reflog.close()
            if args.keep_logs:
                handle = tempfile.mkstemp(dir = '.', prefix = self.suite, suffix = '.out', text = True)[0]
                f = os.fdopen(handle, 'w')
                f.write('\n'.join(orig_lines) + '\n')
                f.close()
            else:
                os.remove(logfile)

def make_suites(args):
    suites = [
        SimpleSuite('string'),
        SimpleSuite('math'),
        SimpleSuite('threads'),
        SimpleSuite('errors'),
        SimpleSuite('interpose'),
        SimpleSuite('procaddress'),
        LogSuite('dlopen', 'trace'),
        LogSuite('draw_client', 'trace'),
        LogSuite('draw_vbo', 'trace'),
        LogSuite('extoverride', 'extoverride'),
        LogSuite('logdebug', 'logdebug'),
        LogSuite('pbo', 'trace'),
        LogSuite('pointers', 'checks'),
        LogSuite('queries', 'trace'),
        LogSuite('setstate', 'trace'),
        LogSuite('showextensions', 'showextensions'),
        LogSuite('texcomplete', 'checks'),
        LogSuite('triangles', 'triangles'),
        LogSuite('ARB_create_context', 'trace'),
        SimpleSuite('contextattribs', 'contextattribs')
    ]

    # Not all suites are necessarily compiled in. Query tester for a list
    out = subprocess.check_output([args.tester, '--list']).decode('utf-8')
    avail = set(out.splitlines())
    out = [x for x in suites if x.suite in avail]
    return out

def parse_args():
    parser = argparse.ArgumentParser(fromfile_prefix_chars = '@')
    parser.add_argument('--library-path', help = 'Directory containing bugle library', metavar = 'PATH')
    parser.add_argument('--filter-dir', help = 'Directory containing filters', metavar = 'PATH')
    parser.add_argument('--filters', help = 'File containing filter config', metavar = 'FILE')
    parser.add_argument('--tester', help = 'Path to bugletest binary', metavar = 'FILE')
    parser.add_argument('--suite', help = 'Test suite to run', metavar = 'SUITE')
    parser.add_argument('--gdb', action = 'store_true', default = False, help = 'Start test suite inside gdb')
    parser.add_argument('--keep-logs', action = 'store_true', default = False, help = 'Do not delete log files')
    args = parser.parse_args()
    if not args.tester:
        parser.error('Argument --tester is required')
    return args

def main():
    args = parse_args()
    suites = make_suites(args)
    if args.suite:
        suites = [x for x in suites if x.suite == args.suite]
        if not suites:
            print('No suite named "' + args.suite + '"', file = sys.stderr)
            return 1

    passed = 0
    failed = 0
    skipped = 0
    for s in suites:
        print('\nRunning ' + s.suite, file = sys.stderr)
        try:
            s.run(args)
        except TestFailed as e:
            msg = e.message
            if msg != '':
                msg = ' (' + msg + ')'
            print("FAILED  " + s.suite + msg, file = sys.stderr)
            failed += 1
        except TestSkipped as e:
            msg = e.message
            if msg != '':
                msg = ' (' + msg + ')'
            print("SKIPPED  " + s.suite + msg, file = sys.stderr)
            skipped += 1
        else:
            print("PASSED  " + s.suite, file = sys.stderr)
            passed += 1

    print('\nTotal: {} passed, {} skipped, {} failed'.format(passed, skipped, failed),
            file = sys.stderr)
    if failed:
        return 1
    else:
        return 0

if __name__ == '__main__':
    sys.exit(main())
