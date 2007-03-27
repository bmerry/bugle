#!/bin/sh
if ! [ -f index.php ]; then
  echo "Please run from html directory" 1>&2
  exit 1
fi
rsync -v -a --copy-links --delete --delete-after --progress --stats --exclude=.svn --exclude=snapshots --exclude='sdk_*.txt' . $HOME/devel/bugle-oglsdk
