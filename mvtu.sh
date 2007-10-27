#!/bin/sh
# Usage: mvtu foo.c bar.tu
set -e
rm -f "$2"
for i in "$1.tu" "$1.t00.tu" "$1.001t.tu"
do
    test -f "$i" && mv -f "$i" "$2"
done
if ! test -f "$2"
then
    echo "No .tu file found! Are you using GCC 3.4 or 4.1+?" 1>&2
    exit 1
fi
