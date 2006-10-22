#!/bin/sh
if ! [ -f index.html ]; then
  echo "Please run from html directory" 1>&2
  exit 1
fi
rsync -v -a --copy-links --delete --delete-after --progress --stats --exclude=.svn --exclude=snapshots . bugle.sf.net:/home/groups/b/bu/bugle/htdocs/
