#!/bin/bash

if [ $# -lt 2 ]; then
    echo "Error: Missing arguments"
    exit 1
fi

writefile=$1
writestr=$2

mkdir -p "$(dirname "$writefile")"

echo "$writestr" > "$writefile"

if [ $? -ne 0 ]; then
    echo "Error: Failed to write to file"
    exit 1
fi

exit 0