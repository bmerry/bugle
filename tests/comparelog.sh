#!/bin/sh
# Run as comparelog.sh program chain
set -e

name="$1"
chain="$2"
BUGLE_CHAIN="$chain" LD_PRELOAD="$BUGLE_LIB" "tests/$name" >"tests/$name.log" 2>&1 3>"tests/$name.base.log"
perl -w "$srcdir/tests/comparelog.perl" "tests/$name.base.log" "tests/$name.log"
rm -f "tests/$name.base.log"
