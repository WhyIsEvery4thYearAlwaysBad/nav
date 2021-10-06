#!/bin/sh
# Primitive test automation.
BINDIR=$(dirname "$(readlink -fn "$0")")
cd "${BINDIR}" || exit 2

for NAVFile in $(find ../test/reference_navs | grep .nav$)
do
	head -c 4 $NAVFile
	printf "\-----------\n%s:\n\n" "$NAVFile"
	../nav info file $NAVFile
done

# Test 'nav compress ids'
for NAVFile in $(ls ../test/reference_navs | grep .nav$)
do
	cp ../test/reference_navs/$NAVFile ../test/copied_navs/

	printf "Running program on %s!" "$NAVFile\n"
	../nav compress ids "../test/copied_navs/$NAVFile"
	printf "Done!\n"
done