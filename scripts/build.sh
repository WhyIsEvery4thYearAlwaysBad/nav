#!/bin/sh
# Build the program.
BINDIR=$(dirname "$(readlink -fn "$0")")
cd "${BINDIR}" || exit 2

COMPILATION="${1:-debug}"
CC="g++-10"
COMPILER_FLAGS="../src/* -static -std=c++2a"

mkdir -p ../builddir
if [ "$COMPILATION" = "release" ]
then
	${CC} $COMPILER_FLAGS -o ../builddir/nav
elif [ "$COMPILATION" = "debug" ]
then
	${CC} -g $COMPILER_FLAGS -o ../builddir/nav
fi
