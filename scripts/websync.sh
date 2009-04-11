#!/bin/sh
if ! [ -f html/index.php ]; then
  echo "Please run from top-level directory" 1>&2
  exit 1
fi
rsync -v -a --copy-links --delete --delete-after --progress --stats --exclude=.svn --exclude=documentation --exclude=snapshots --exclude='sdk_*.txt' html/ $HOME/devel/bugle/oglsdk
xsltproc --xinclude --stringparam "base.dir" "$HOME/devel/bugle/oglsdk/documentation/" doc/DocBook/xhtml-chunk-php.xsl doc/DocBook/bugle.xml
