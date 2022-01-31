#!/usr/bin/ksh
#
# getweather - gets the latest weather GIF image file from the anonymous ftp 
# area of the server machine (SERVERHOST), in the directory (DIRNAME)
# and copies it to the file WEATHERFILE if successful
#
# this should be run hourly, on the half-hour,  in a crontab entry
#
# John Bradley, 03/26/92
#
# based on getnettables, by John Dotts Hagan
##
## Modified by Marc Evans to allow for the specification of the file type
## desired. The types are:
##
##    -i    CImmddhh.GIF    Infared cloud image (Cloud Infared)
##    -v    CVmmddhh.GIF    Visual cloud image (Cloud Visual)
##    -g    SAmmddhh.GIF    Ground based weather (Surface Analysis)
##    -u2   U2mmddhh.GIF    Wind characteristics at 2000 feet?
##    -u3   U3mmddhh.GIF    Wind characteristics at 3000 feet?
##    -u5   U5mmddhh.GIF    Wind characteristics at 5000 feet?
##    -u7   U7mmddhh.GIF    Wind characteristics at 7000 feet?
##    -u8   U8mmddhh.GIF    Wind characteristics at 8000 feet?

## Default paramete
SERVERHOST=vmd.cso.uiuc.edu
DIRNAME=wx
ACCT=anonymous
PASS=zk3.dec.com
HOSTMANAGER=root
FTP=gate-ftp

## Start up the ftp process
$FTP -n -i -v $SERVERHOST |&

## Loop through all of the options
for i in $* ; do

    ## Allow the command line to specify the type of image to get
    case "$i" in
	-i)    PREFIX="CI";;
	-v)    PREFIX="CV";;
	-g)    PREFIX="SA";;
	-u2)   PREFIX="U2";;
	-u3)   PREFIX="U3";;
	-u5)   PREFIX="U5";;
	-u7)   PREFIX="U7";;
	-u8)   PREFIX="U8";;
	*)     PREFIX="SA";;
    esac

    ## Determine the most recent file of the type requested
    ## (Note that we can't assume that the ls command will sort for us)
    print -p "user $ACCT $PASS"
    print -p "cd $DIRNAME"
    print -p "ls $PREFIX*"
    FNAME=`while read -p line ; do
	if [ "$line" = "250 List completed successfully." ] ; then
	    break
	fi
	echo $line
    done | sort | egrep "$PREFIX[0-9]+" | tail -1 | sed 's/ GIF.*/.GIF/'`
    TARGET="/tmp/$FNAME"

    ## Go get the file if needed
    if [ ! -f $TARGET ]; then
	print -p "binary"
	print -p "get $FNAME $TARGET"
    fi

    ## Output the name of the most recent file
    echo $TARGET
done

## Complete the process
print -p "quit"
wait
exit 0
