#!/bin/bash
FILE=$1
WIDTH=$4
HEIGHT=$5
XPOS=$2
YPOS=$3
if [[ $XPOS -lt 0 ]]; then
	echo "Negative x position $XPOS converted to zero, width adjusted."
	WIDTH=$((WIDTH+XPOS))
	XPOS=0
fi
if [[ $YPOS -lt 0 ]]; then
	echo "Negative y position $YPOS converted to zero, height adjusted."
	HEIGHT=$((HEIGHT+YPOS))
	YPOS=0
fi
if [[ $(($WIDTH+$HEIGHT)) == 0 ]]; then
	echo "Refusing to crop $FILE with an empty rectangle."
	exit 1
fi

TMPFILE=`mktemp $FILE.tmp-jpeg-rot-XXXXXXXX`
if jpegtran -optimize -progressive -crop ${WIDTH}x${HEIGHT}+${XPOS}+${YPOS} \
   -copy all $FILE > $TMPFILE; then
	COUNT=`echo $FILE-uncropped | wc -c`
	NEXT=$((`ls $FILE-uncropped* | cut -b $COUNT- | grep '^[0-9]*$' \
	         | sed 's/^0*/'/ | sort -n | tail -n 1`+1))
	# the targets shouldn't exist, but -i just in case
	mv -i $FILE $FILE-uncropped$NEXT 
	mv -i $TMPFILE $FILE
else
	rm $TMPFILE
fi
