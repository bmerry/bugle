#!/bin/sh
if ! [ -d doc ]; then
  echo "Please run me from the top-level directory" 1>&2
  exit 1
fi

for i in man1 man3 man7; do
  for j in doc/$i/*; do
    base=$(basename $j)
    changed=$(svn info $j | grep '^Last Changed Date: ')
    (cd doc/$i && man2html -r "$base") | tail -n +3 | sed "s/^Time: .*/$changed/" > "html/documentation/$i/$base.html"
  done
done
