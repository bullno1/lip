C_FLAGS ?= -O3 -std=c99 -flto -Wall -Wextra -Werror -pedantic
CPP_FLAGS ?= -Wall -Werror -Wextra -pedantic
LINKER ?= cc
LINK_FLAGS ?= -g -O3 -flto

-import cpp.nu

all: tests bin/lip ! live

tests: bin/tests ! live
	echo "-------------------------------------"
	valgrind --leak-check=full bin/tests --no-fork

test:%: bin/tests ! live
	echo "-------------------------------------"
	valgrind --leak-check=full bin/tests --no-fork ${m}

bin/tests: << C_FLAGS CPP_FLAGS
	${NUMAKE} exe:$@ \
		sources="`find src/tests -name '*.cpp' -or -name '*.c'`" \
		c_flags="${C_FLAGS} -g -Isrc" \
		cpp_flags="${CPP_FLAGS} -Isrc" \
		libs="bin/liblip.a"

bin/lip: << C_FLAGS CPP_FLAGS
	${NUMAKE} exe:$@ \
		sources="`find src/repl -name '*.cpp' -or -name '*.c'`" \
		c_flags="${C_FLAGS} -Isrc" \
		cpp_flags="${CPP_FLAGS} -Isrc" \
		libs="bin/liblip.a"

bin/liblip.a:
	${NUMAKE} static-lib:$@ \
		sources="src/lip/lexer.c src/lip/array.c src/lip/memory.c src/lip/io.c"

# Only for vm_dispatch.c, remove -pedantic because we will be using a
# non-standard extension (computed goto) if it is available
$BUILD_DIR/src/lip/vm_dispatch.c.o: src/lip/vm_dispatch.c << COMPILE c_compiler C_COMPILER c_flags C_FLAGS
	${NUMAKE} --depend ${COMPILE} # Compilation depends on the compile script too
	${COMPILE} "${deps}" "$@" "${c_compiler:-${C_COMPILER}}" "$(echo ${c_flags:-${C_FLAGS}} | sed 's/-pedantic//g')"
