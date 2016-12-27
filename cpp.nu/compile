#!/bin/sh -e

# compile.sh <source> <obj> <compiler> <flags>

`${NUMAKE} --enable-trace`

# Compile source to object file
mkdir -p $(dirname $2)
echo $3 -c $1 -o $2 $4 -MMD -MF $2.d
$3 -c $1 -o $2 $4 -MMD -MF $2.d

# Add headers dependency
headers=$(sed -e :a -e '/\\$/N; s/\\\n//; ta' "$2.d" | cut -d' ' -f3-)
${NUMAKE} --depend ${headers}
