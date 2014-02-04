To build the Win64 binary, install the "Windows 2003 Server R2" version of the
platform SDK and open a command line window. Change directory to the
par2cmdline-0.4-tbb-<version> directory. Move or copy the Makefile in this
directory to its parent (ie, to the par2cmdline-0.4-tbb-<version> directory).
Then use the 'nmake' command to build the binary. The result should be a file
named par2_win64.exe in the par2cmdline-0.4-tbb-<version> directory. This can
then be renamed to par2.exe if so desired.

More information is in the Makefile's comments.

Vincent Tan.
February 3, 2010.
