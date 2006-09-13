#!/bin/sh
if ! [ -d html ]; then
  echo "Please run from parent of html dir"
  exit 1
fi

exec rsync --exclude=.svn -v -a -L html/* bugle.sf.net:htdocs/
