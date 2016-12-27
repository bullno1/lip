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
UB_FLAGS_1 = -fsanitize=undefined -fno-sanitize-recover=undefined
UB_FLAGS = $(eval echo \${UB_FLAGS_$WITH_UB_SAN})

WITH_ADDR_SAN ?= 0
ADDR_SAN_FLAGS_0 =
ADDR_SAN_FLAGS_1 = -fsanitize=address
ADDR_SAN_FLAGS = $(eval echo \${ADDR_SAN_FLAGS_$WITH_ADDR_SAN})

COMMON_FLAGS = -Iinclude -pthread ${UB_FLAGS} ${ADDR_SAN_FLAGS} ${COVERAGE_FLAGS} ${OPTIMIZATION_FLAGS} ${LTO_FLAGS}
COMPILATION_FLAGS = -g -Wall -Wextra -Werror -pedantic
C_FLAGS ?= -std=c99 ${COMPILATION_FLAGS} ${COMMON_FLAGS}
CPP_FLAGS ?= ${COMPILATION_FLAGS} ${COMMON_FLAGS}
LINK_FLAGS ?= -g ${COMMON_FLAGS}

-import cpp.nu

all: tests bin/lip ! live

tests: bin/tests ! live
	echo "-------------------------------------"
	bin/tests --color always

test:%: bin/tests ! live << SEED
	echo "-------------------------------------"
	if [ -z "${SEED}" ]; then
		bin/tests --color always ${m}
	else
		bin/tests --color always --seed ${SEED} ${m}
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
		--exclude '.*src/lip/vendor/.*' \
		--exclude '.*src/lip/khash_impl.c' \
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
		c_flags="${C_FLAGS} -Ideps/linenoise-ng/include" \
		cpp_flags="${CPP_FLAGS} -Ideps/linenoise-ng/include" \
		libs="bin/liblip.a bin/liblinenoise-ng.a"

bin/liblip.a:
	${NUMAKE} static-lib:$@ \
		sources="`find src/lip -name '*.cpp' -or -name '*.c'`"

bin/liblinenoise-ng.a:
	${NUMAKE} static-lib:$@ \
		c_flags="${C_FLAGS} -Ideps/linenoise-ng/include" \
		cpp_flags="${CPP_FLAGS} -Ideps/linenoise-ng/include" \
		sources="`find deps/linenoise-ng -name '*.cpp' -or -name '*.c'`"

# Only for vm_dispatch.c, remove -pedantic because we will be using a
# non-standard extension (computed goto) if it is available
$BUILD_DIR/%/src/lip/vm_dispatch.c.o: src/lip/vm_dispatch.c << COMPILE cc CC c_flags C_FLAGS
	${NUMAKE} --depend ${COMPILE} # Compilation depends on the compile script too
	${COMPILE} "${deps}" "$@" "${cc:-${CC}}" "$(echo ${c_flags:-${C_FLAGS}} | sed 's/-pedantic//g')"
