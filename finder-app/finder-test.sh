#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Siddhant Jajoo

set -e # Exit immediately if a command exits with a non-zero status
set -u # Treat unset variables as an error

# Initialize IS_ASSIGNMENT_4_PART_2 to false for assignment-3-and-later.
# Or Initialize IS_ASSIGNMENT_4_PART_2 to true for assignment-4-part-2 
IS_ASSIGNMENT_4_PART_2=${IS_ASSIGNMENT_4_PART_2:-true}
NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data

# Set conf path according to variable IS_ASSIGNMENT_4_PART_2
if $IS_ASSIGNMENT_4_PART_2; then
    CONFIG_DIR="/etc/finder-app/conf"
else
    CONFIG_DIR="conf"
fi

username=$(cat "$CONFIG_DIR/username.txt")

if [ $# -lt 3 ]
then
	echo "Using default value ${WRITESTR} for string to write"
	if [ $# -lt 1 ]
	then
		echo "Using default value ${NUMFILES} for number of files to write"
	else
		NUMFILES=$1
	fi	
else
	NUMFILES=$1
	WRITESTR=$2
	WRITEDIR=/tmp/aeld-data/$3
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string ${WRITESTR} to ${WRITEDIR}"

rm -rf "${WRITEDIR}"

# create $WRITEDIR if not assignment1
assignment=$(cat "$CONFIG_DIR/assignment.txt")

if [ $assignment != 'assignment1' ]
then
	mkdir -p "$WRITEDIR"

	#The WRITEDIR is in quotes because if the directory path consists of spaces, then variable substitution will consider it as multiple argument.
	#The quotes signify that the entire string in WRITEDIR is a single string.
	#This issue can also be resolved by using double square brackets i.e [[ ]] instead of using quotes.
	if [ -d "$WRITEDIR" ]
	then
		echo "$WRITEDIR created"
	else
		exit 1
	fi
fi

#echo "Removing the old writer utility and compiling as a native application"
#make clean
#make

for i in $( seq 1 $NUMFILES)
do
	if $IS_ASSIGNMENT_4_PART_2; then
		writer "$WRITEDIR/${username}$i.txt" "$WRITESTR" # assuming executable in is the PATH
	else
		./writer "$WRITEDIR/${username}$i.txt" "$WRITESTR"
	fi
done

if $IS_ASSIGNMENT_4_PART_2; then
	OUTPUTSTRING=$(finder.sh "$WRITEDIR" "$WRITESTR") # assuming executable in is the PATH 
else
	OUTPUTSTRING=$(./finder.sh "$WRITEDIR" "$WRITESTR")
fi

if $IS_ASSIGNMENT_4_PART_2; then
    echo "${OUTPUTSTRING}" > /tmp/assignment4-result.txt # write a file with output of the finder command
fi

# remove temporary directories
rm -rf /tmp/aeld-data

set +e
echo ${OUTPUTSTRING} | grep "${MATCHSTR}"
if [ $? -eq 0 ]; then
	echo "success"
	exit 0
else
	echo "failed: expected  ${MATCHSTR} in ${OUTPUTSTRING} but instead found"
	exit 1
fi