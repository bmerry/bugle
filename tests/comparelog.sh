#!/bin/sh
# Run as comparelog.sh program
set -e
BUGLE_CHAIN=trace tests/$1 2>&1 | sed 's/0x[0-9a-f][0-9a-f]*/0x????????/g' > tests/$1.log
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
