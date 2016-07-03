WITH_COVERAGE ?= 0
COVERAGE_0 =
COVERAGE_1 = -fprofile-arcs -ftest-coverage
COVERAGE_FLAGS = $(eval echo \${COVERAGE_$WITH_COVERAGE})
OPTIMIZATION_0 = -O3
OPTIMIZATION_1 = -O0
OPTIMIZATION_FLAGS = $(eval echo \${OPTIMIZATION_$WITH_COVERAGE})
C_FLAGS ?= -g -std=c99 -flto -Wall -Wextra -Werror -pedantic ${COVERAGE_FLAGS} ${OPTIMIZATION_FLAGS}
CPP_FLAGS ?= -g -Wall -Werror -Wextra -pedantic ${COVERAGE_FLAGS} ${OPTIMIZATION_FLAGS}
LINK_FLAGS ?= -g -flto ${COVERAGE_FLAGS} ${OPTIMIZATION_FLAGS}

-import cpp.nu

info: << CC CXX AR ! live
	echo CC=${CC}
	echo CXX=${CXX}
	echo AR=${AR}

all: tests bin/lip ! live

tests: bin/tests ! live
	echo "-------------------------------------"
	valgrind --leak-check=full bin/tests --no-fork

test:%: bin/tests ! live
	echo "-------------------------------------"
	valgrind --leak-check=full bin/tests --no-fork ${m}

cover: tests
	mkdir -p $@
	gcovr -r . -d -f '.*src/lip/.*' --html --html-details -o $@/index.html

bin/tests: << C_FLAGS CPP_FLAGS CC
	${NUMAKE} exe:$@ \
		sources="`find src/tests -name '*.cpp' -or -name '*.c'`" \
		c_flags="${C_FLAGS} -g -Isrc" \
		cpp_flags="${CPP_FLAGS} -Isrc" \
		linker="${CC}" \
		libs="bin/liblip.a"

bin/lip: << C_FLAGS CPP_FLAGS CC
	${NUMAKE} exe:$@ \
		sources="`find src/repl -name '*.cpp' -or -name '*.c'`" \
		c_flags="${C_FLAGS} -Isrc" \
		cpp_flags="${CPP_FLAGS} -Isrc" \
		linker="${CC}" \
		libs="bin/liblip.a"

bin/liblip.a:
	${NUMAKE} static-lib:$@ \
		sources="src/lip/lexer.c src/lip/array.c src/lip/memory.c src/lip/io.c"

# Only for vm_dispatch.c, remove -pedantic because we will be using a
# non-standard extension (computed goto) if it is available
$BUILD_DIR/src/lip/vm_dispatch.c.o: src/lip/vm_dispatch.c << COMPILE c_compiler C_COMPILER c_flags C_FLAGS
	${NUMAKE} --depend ${COMPILE} # Compilation depends on the compile script too
	${COMPILE} "${deps}" "$@" "${c_compiler:-${C_COMPILER}}" "$(echo ${c_flags:-${C_FLAGS}} | sed 's/-pedantic//g')"
