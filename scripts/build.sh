#!/bin/sh
# Build the program.
BINDIR=$(dirname "$(readlink -fn "$0")")
cd "${BINDIR}" || exit 2
cd ../src

COMPILATION="${1:-debug}"
CC="g++-10"
COMPILER_FLAGS="$(find . -type f) -Isrc/include -std=c++2a -static"

mkdir -p ../builddir
if [ "$COMPILATION" = "release" ]
then
	${CC} $COMPILER_FLAGS -o ../builddir/nav
elif [ "$COMPILATION" = "debug" ]
then
	${CC} $COMPILER_FLAGS -g -o ../builddir/nav
fi
