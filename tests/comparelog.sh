#!/bin/sh
# Run as comparelog.sh program
set -e

if [ -n "$2" ]; then chain="$2"; else chain=trace; fi
BUGLE_CHAIN="$chain" tests/$1 2>&1 | sed -e 's/0x[0-9a-f][0-9a-f]*/0x????????/g' -e 's/^stats.fps: .*$/stats.fps: <suppressed>/' > tests/$1.log
if [ -r tests/$1.base.log ]; then
  echo "Comparing log for $1 to base"
  if ! diff tests/$1.base.log tests/$1.log > /dev/null; then
    echo "Logs differ. This may be a regression."
    exit 1
  fi
else
  echo "Saving baseline log for $1"
  cp tests/$1.log tests/$1.base.log
fi
