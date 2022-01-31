#!/bin/bash
curdir="`pwd`"
LOG=~/photos/bin/rotscript # customize your install dir here
if [[ ! -e "$LOG" ]]; then
	echo '#!/bin/bash' >> "$LOG"
	chmod u+x "$LOG"
fi
echo "# following entry made on `date`" >> "$LOG"
# also customize the following line
echo ~/photos/bin/jpegrot \""$1"\" \""$curdir/$2"\" >> "$LOG"
