XV Software
===========

This directory hierarchy contains the source code for the XV software.

Other additional sources of information on the XV software include:

  - <http://www.trilon.com/xv/>

  - <https://en.wikipedia.org/wiki/Xv_(software)>

History of This Code
====================

This source code was originally derived from the following:

  - <ftp://ftp.trilon.com/pub/xv/xv-3.10a.tar.gz>
    (obtained 2022-01-30)

  - <http://prdownloads.sourceforge.net/png-mng/xv-3.10a-jumbo-patches-20070520.tar.gz>
    (obtained 2022-01-30)

  - <http://www.gregroelofs.com/code/xv-3.10a-enhancements.20070520-20081216.diff>
    (obtained 2022-01-30)

Many changes/improvements have been made on top of the original code.
Some of these changes/improvements include (but are not limited to):

  - merging numerous patches from the Fedora Linux RPM for the XV software
    on RPM Fusion;

  - adding support for CMake-based builds; and

  - adding a continuous integration (CI) workflow based on GitHub Actions,
    which builds xv using a combination of platforms/compilers
    (e.g., GCC/Clang on Ubuntu/MacOS).

Acknowledgments
===============

The XV software was originally written by John Bradley.
John Bradley's web site for the XV software can be found at:

  - <http://www.trilon.com/xv>

The creator of this GitHub repository (Michael Adams) would like to thank
John Bradley for giving his blessing for a public Git repository for the
XV software.  It is hoped that this repository will allow the ongoing
support and maintenance of the XV software to be done more easily.
