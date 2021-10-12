#!/bin/sh
# Primitive test automation.
BINDIR=$(dirname "$(readlink -fn "$0")")
cd "${BINDIR}" || exit 2

# Should return nothing.
nav > /dev/null
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
cp -rf test/reference_navs test/copied_navs

# Test Count
TEST_COUNT=0
# Amount of regular failures
PROG_DEFINED_FAILURES=0
# Exceptions
FATAL_FAILURES=0

COMMANDS="create edit delete info"
# Targets
TARGETS="area 0|area 0 connection 0 0|area 0 connection 1 0|area 0 connection 2 0|area 0 connection 3 0|area 0 connection 4 0|area 0 encounter-path 0|area 0 encounter-spot 0|area 0 encounter-path 0|encounter-spot 0"
for NAVFile in $(ls test/copied_navs | grep .nav$)
do
	printf "\---------\$(nav file \"%s\" %s %s)--------\n" "$NAVFile" "$Target" "$Command"
	for Command in $COMMANDS
	do
		while read -r Target
		do
			nav file test/copied_navs/"$NAVFile" $Target $Command #>/dev/null #2>&1
			STATUS=$?
			# Increment test count.
			TEST_COUNT=$((TEST_COUNT+1))
			printf "\$(nav file \"%s\" %s %s): " "$NAVFile" "$Target" "$Command" 
			# Print error code.
			if [ $STATUS -eq 0 ]
			then
				printf "\033[32m%i (Success.)" "$STATUS"
			elif [ $STATUS -eq 1 ]
			then
				printf "\033[31m%i (Program-defined failure.)" "$STATUS"
				PROG_DEFINED_FAILURES=$((PROG_DEFINED_FAILURES+1))
			# Not a program-defined error. FUCK!
			else
				printf "\033[1m\033[91m%i (Fatal Failure!)" $STATUS
				FATAL_FAILURES=$((FATAL_FAILURES+1))
			fi
			printf "\033[0m\n"
		done <<Doc
$(printf "%s" "$TARGETS" | tr '|' '\n')
Doc
	done
done

# Printout results.
printf "\n------Tests Ran: %i-----\n" "$TEST_COUNT"
printf "\033[1m\033[91m%i fatal failures!\033[0m\n" "$FATAL_FAILURES"