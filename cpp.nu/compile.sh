#!/bin/sh -e

# compile.sh <source> <obj> <compiler> <flags>

`${NUMAKE} --enable-trace`

# Compile source to object file
${NUMAKE} --order-only dir:$(dirname $2)/
$3 -c $1 -o $2 $4 -MMD -MF $2.d

# Add headers dependency
headers=
for file in `cat $2.d`
do
	case ${file} in
		\\|*:|$1)
			continue
			;;
		*)
			headers="${headers} ${file}"
			;;
	esac
done
${NUMAKE} --depend ${headers}
