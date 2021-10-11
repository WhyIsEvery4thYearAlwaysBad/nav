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

# Good. Run test command.

nav test

STATUS=$?

if [ $STATUS -eq 0 ]
then
	printf "\$(nav test): Pass!\n"
else
	printf "\$(nav test): Fail!\n"
fi

# Operate on each NAV.
cd ..
cp -f test/reference_navs test/copied_navs

for NAVFile in $(ls test/copied_navs | grep .nav$)
do
	STATUS=$(nav file "test/copied_navs/$NAVFile" info)
	
	printf "\$(nav file \"$NAVFile\"): %i\n" $STATUS
done