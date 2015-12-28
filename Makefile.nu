C_FLAGS ?= -g -Wall -Werror -pedantic
CPP_FLAGS ?= -Wall -Werror -pedantic

-import cpp.nu

all: tests ! live

tests: ! live
	for file in `find src/tests -name '*.c' -or -name '*.cpp'`
	do
		${NUMAKE} --depend test:$(basename ${file%%.*})
	done

test:%: ! live
	${NUMAKE} --depend exec:bin/tests/${m}

bin/tests/%: << C_FLAGS CPP_FLAGS
	${NUMAKE} exe:$@ \
		sources="src/tests/${m}.c" \
		c_flags="${C_FLAGS} -Isrc" \
		cpp_flags="${CPP_FLAGS} -Isrc" \
		libs="bin/liblip.a"

bin/liblip.a:
	${NUMAKE} static-lib:$@ \
		sources="`find src/lip -name '*.cpp' -or -name '*.c'`"

exec:%: % ! live
	echo "-------------------------------------"
	valgrind --leak-check=full ${m}
	echo "-------------------------------------"

$BUILD_DIR/src/lip/vm_dispatch.c.o: src/lip/vm_dispatch.c << COMPILE c_compiler C_COMPILER c_flags C_FLAGS
	${NUMAKE} --depend ${COMPILE} # Compilation depends on the compile script too
	${COMPILE} "${deps}" "$@" "${c_compiler:-${C_COMPILER}}" "$(echo ${c_flags:-${C_FLAGS}} | sed 's/-pedantic//g')"
