WITH_COVERAGE ?= 0
COVERAGE_0 =
COVERAGE_1 = --coverage
COVERAGE_FLAGS = $(eval echo \${COVERAGE_$WITH_COVERAGE})

OPTIMIZATION_0 = -O3
OPTIMIZATION_1 = -O0
OPTIMIZATION_FLAGS = $(eval echo \${OPTIMIZATION_$WITH_COVERAGE})

WITH_LTO ?= 1
LTO_0 =
LTO_1 = -flto
LTO_FLAGS = $(eval echo \${LTO_$WITH_LTO})

WITH_UB_SAN ?= 0
UB_FLAGS_0 =
UB_FLAGS_1 = -fsanitize=undefined
UB_FLAGS = $(eval echo \${UB_FLAGS_$WITH_UB_SAN})

COMMON_FLAGS = -g ${UB_FLAGS} ${COVERAGE_FLAGS} ${OPTIMIZATION_FLAGS} ${LTO_FLAGS}
COMPILATION_FLAGS = -Wall -Wextra -Werror -pedantic
C_FLAGS ?= -std=c99 ${COMPILATION_FLAGS} ${COMMON_FLAGS}
CPP_FLAGS ?= ${COMPILATION_FLAGS} ${COMMON_FLAGS}
LINK_FLAGS ?= -g ${COMMON_FLAGS}

VALGRIND ?= valgrind

-import cpp.nu

all: tests bin/lip ! live

tests: bin/tests ! live << VALGRIND UB_FLAGS
	echo "-------------------------------------"
	${VALGRIND} bin/tests

test:%: bin/tests ! live << VALGRIND SEED
	echo "-------------------------------------"
	if [ -z "${SEED}" ]; then
		${VALGRIND} bin/tests ${m}
	else
		${VALGRIND} bin/tests --seed ${SEED} ${m}
	fi

cover: tests
	mkdir -p $@
	gcovr \
		--root . \
		--branches \
		--exclude-unreachable-branches \
		--sort-percentage \
		--delete \
		--filter '.*src/lip/.*' \
		--html --html-details \
		--output $@/index.html

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
	MODULES=" \
		lexer \
		array \
		memory \
		io \
		parser \
		asm \
		temp_allocator \
	"
	SOURCES=$(
		echo ${MODULES} |
		awk '{ for(i = 1; i <= NF; i++) { print "src/lip/" $i ".c"; } }'
	)
	${NUMAKE} static-lib:$@ sources="${SOURCES}"

# Only for vm_dispatch.c, remove -pedantic because we will be using a
# non-standard extension (computed goto) if it is available
$BUILD_DIR/src/lip/vm_dispatch.c.o: src/lip/vm_dispatch.c << COMPILE c_compiler C_COMPILER c_flags C_FLAGS
	${NUMAKE} --depend ${COMPILE} # Compilation depends on the compile script too
	${COMPILE} "${deps}" "$@" "${c_compiler:-${C_COMPILER}}" "$(echo ${c_flags:-${C_FLAGS}} | sed 's/-pedantic//g')"
