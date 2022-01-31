#!/bin/bash
FILE=$1
COUNT=`echo $FILE-uncropped | wc -c`
LATEST=$((`ls $FILE-uncropped* | cut -b $COUNT- | grep '^[0-9]*$' \
         | sed 's/^0*/'/ | sort -n | tail -n 1`))
if [[ -f $FILE-uncropped$LATEST ]]; then
	mv $FILE-uncropped$LATEST $FILE
fi
