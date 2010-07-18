#!/usr/bin/env python
import pysvn
import datetime, time
import re

def year_start(year):
    return int(time.mktime(datetime.datetime(year, 1, 1).timetuple()))

client = pysvn.Client()

rev1 = pysvn.Revision(pysvn.opt_revision_kind.date, year_start(2010))
rev2 = pysvn.Revision(pysvn.opt_revision_kind.head)
peg = pysvn.Revision(pysvn.opt_revision_kind.head)
working = pysvn.Revision(pysvn.opt_revision_kind.working)

kinds = [
    pysvn.diff_summarize_kind.modified,
    pysvn.diff_summarize_kind.added]

copy_pattern = re.compile(r'Copyright \(C\) ([-0-9, ].*) Bruce Merry')
year_pattern = re.compile('2010')

summary = client.diff_summarize_peg('.', peg, rev1, rev2)
for d in summary:
    if d.node_kind == pysvn.node_kind.file and d.summarize_kind in kinds:
        text = client.cat(d.path, working)
        matches = copy_pattern.search(text)
        if matches is not None:
            if not year_pattern.search(matches.group(1)):
                print "%s: %s" % (d.path, matches.group(1))
                try:
                    diff = client.diff_peg('/tmp', d.path, peg, rev1, rev2, False)
                    print diff
                except pysvn.ClientError:
                    print '(could not get diff - added?)'
