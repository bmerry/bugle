#!/usr/bin/env python

"""Find files whose copyright date potentially needs changing"""

# TODO: use diff command to check whether content has actually changed.

import sys
import os.path
import getopt
import svn.core, svn.client, svn.wc
import datetime
import re
from StringIO import StringIO

def do_status(wc_path):
    # Build a client context baton.
    ctx = svn.client.svn_client_ctx_t()
    revision = svn.core.svn_opt_revision_t()
    revision.type = svn.core.svn_opt_revision_unspecified

    pattern = re.compile(r'Copyright \(C\) ([-0-9, ].*) Bruce Merry')
    yearpattern = re.compile('2009')

    def _info_callback(path, info, pool):
        """A callback function for svn_client_info2."""

        dt = datetime.datetime.utcfromtimestamp(info.last_changed_date / 1000000.0)
        if info.kind == svn.core.svn_node_file and dt.year == 2009:
            io = StringIO()
            svn.client.svn_client_cat2(io, path, revision, revision, ctx, pool)
            text = io.getvalue()
            matches = pattern.search(text)
            if matches is not None:
                if not yearpattern.search(matches.group(1)):
                    print "%s: %s" % (path, matches.group(1))

    # Do the status crawl, using _status_callback() as our callback function.
    svn.client.svn_client_info2(wc_path, revision, revision,
                                _info_callback,
                                svn.core.svn_depth_infinity,
                                None,
                                ctx)

def usage_and_exit(errorcode):
    """Print usage message, and exit with ERRORCODE."""
    stream = errorcode and sys.stderr or sys.stdout
    stream.write("""Usage: %s OPTIONS WC-PATH
Options:
  --help, -h    : Show this usage message
""" % (os.path.basename(sys.argv[0])))
    sys.exit(errorcode)

if __name__ == '__main__':
    # Parse command-line options.
    try:
        opts, args = getopt.getopt(sys.argv[1:], "h", ["help"])
    except getopt.GetoptError:
        usage_and_exit(1)
    verbose = 0
    for opt, arg in opts:
        if opt in ("-h", "--help"):
            usage_and_exit(0)
    if len(args) != 1:
        usage_and_exit(2)

    # Canonicalize the repository path.
    wc_path = svn.core.svn_path_canonicalize(args[0])

    # Do the real work.
    try:
        do_status(wc_path)
    except svn.core.SubversionException, e:
        sys.stderr.write("Error (%d): %s\n" % (e.apr_err, e.message))
        sys.exit(1)
