#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir
PROJECT=Flow
TEST_TYPE=-f
FILE=flow/flow.h
DIE=0

(gtkdocize --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have gtk-doc installed to compile $PROJECT."
	echo "Install the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnome.org/pub/GNOME/sources/gtk-doc/"
	DIE=1
}

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $PROJECT."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to compile $PROJECT."
	echo "Get ftp://sourceware.cygnus.com/pub/automake/automake-1.4.tar.gz"
	echo "(or a newer version if it is available)"
	DIE=1
}

if test "$DIE" -eq 1; then
	exit 1
fi

test $TEST_TYPE $FILE || {
	echo "You must run this script in the top-level $PROJECT directory"
	exit 1
}

if test -z "$*"; then
	echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
fi

am_opt="--include-deps --add-missing"

echo "Running libtoolize..."
if [ -x glibtoolize ]; then
  glibtoolize --force --copy || exit $?
else
  libtoolize --force --copy || exit $?
fi

echo "Running gtkdocize..."
gtkdocize || exit $?

echo "Running aclocal..."
aclocal $ACLOCAL_FLAGS || exit $?

# optionally feature autoheader
(autoheader --version)  < /dev/null > /dev/null 2>&1 && autoheader

echo "Running automake..."
automake -a $am_opt || exit $?

echo "Running autoconf..."
autoconf || exit $?

cd $ORIGDIR
$srcdir/configure --enable-maintainer-mode "$@"

echo 
echo "Now type 'make' to compile $PROJECT."
