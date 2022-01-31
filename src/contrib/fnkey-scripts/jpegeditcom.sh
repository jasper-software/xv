#!/bin/bash
CFILE=`mktemp $1.tmp-jpeg-comments-XXXXXXXX`
TMPFILE=`mktemp $1.tmp-jpeg-XXXXXXXX`
rdjpgcom $1 > $CFILE
INITIAL=`md5sum $CFILE`
xterm -e "$EDITOR" "$CFILE" # customize your editor here
MODIFIED=`md5sum $CFILE`
if [[ "$INITIAL" != "$MODIFIED" ]]; then
	mv $1 $TMPFILE
	wrjpgcom -replace -cfile $CFILE $TMPFILE > $1
fi
rm $TMPFILE $CFILE $CFILE~
