#!/bin/sh -f
#
# tests to see if the program 'ranlib' exists.  If it does, runs ranlib on
# the first argument (a library name).  Otherwise, does nothing, and returns
#
# written by John Bradley for the XV 3.00 release
# thanks to John Hagan for shell-script hackery
#

echo "executing 'ranlib $1'..."

# Is there a ranlib?  Let's try and then suffer the consequences...
ranlib $1 >& /dev/null

if [ $? -ne 0  ]; then
	echo "There doesn't seem to be a ranlib on this system..."
	echo "Don't worry about it."
fi

echo ""
echo ""

