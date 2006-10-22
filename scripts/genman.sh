#!/bin/sh
man2html -r "$1" | tail -n +3
