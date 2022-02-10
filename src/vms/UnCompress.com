$! UnCompress.com
$!
$!  A command script for XV to allow VMS users to view GZip'd files 'on
$!  the fly'.  Because GUnZip assumes the host can do re-direction, it is
$!  necessary to redefine the StdOut to a temp text file before executing
$!  GUnZip
$!
$!  The goal is to make this act like the following Unix command, leaving
$!  the original compressed file unchanged:
$!
$!      gunzip -c compresed_file uncompressed_file
$!
$   Write sys$output "Using VMS GUnZip..."
$   Define /NoLog /User_Mode Sys$Output 'P2'
$   GUnZip -c 'P1'
$   Exit
