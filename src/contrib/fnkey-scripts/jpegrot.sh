#!/bin/bash
TMPFILE="`mktemp "$2".tmp-jpeg-rot-XXXXXXXX`"
if jpegtran -perfect -optimize -progressive -rotate "$1" -copy all "$2" > "$TMPFILE"; then
	mv "$TMPFILE" "$2"
else
	rm "$TMPFILE"
fi
