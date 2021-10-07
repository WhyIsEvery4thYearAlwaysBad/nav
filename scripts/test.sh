#!/bin/sh
# Primitive test automation.
BINDIR=$(dirname "$(readlink -fn "$0")")
cd "${BINDIR}" || exit 2

# Should return nothing.
nav

STATUS=$?

if [ $STATUS -eq 0 ]
then
	printf 'Blank $(nav): Pass!'
else
	printf 'Blank $(nav): Fail!'
fi