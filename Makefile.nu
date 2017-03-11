GENERATE_CONFIG_H ?= 1
LIP_CONFIG_H_0 =
LIP_CONFIG_H_1 = include/lip/gen/config.h
LIP_CONFIG_H = $(eval echo \${LIP_CONFIG_H_$GENERATE_CONFIG_H})
LIP_CONFIG_EXTRA_FLAGS_0 =
LIP_CONFIG_EXTRA_FLAGS_1 = -DLIP_HAVE_CONFIG_H
LIP_CONFIG_EXTRA_FLAGS = $(eval echo \${LIP_CONFIG_EXTRA_FLAGS_$GENERATE_CONFIG_H})

BUILD_DYNAMIC_LIB ?= 1
LIBLIP_0 = bin/liblip.a
LIBLIP_1 = bin/liblip.so
LIBLIP = $(eval echo \${LIBLIP_$BUILD_DYNAMIC_LIB})
LIB_EXTRA_FLAGS_0 = -DLIP_DYNAMIC=0
LIB_EXTRA_FLAGS_1 = -DLIP_DYNAMIC=1
LIBLIP_EXTRA_FLAGS = $(eval echo \${LIB_EXTRA_FLAGS_$BUILD_DYNAMIC_LIB})

WITH_COMPUTED_GOTO ?= 1
COMPUTED_GOTO_0 = -DLIP_NO_COMPUTED_GOTO
COMPUTED_GOTO_1 =
COMPUTED_GOTO_FLAGS = $(eval echo \${COMPUTED_GOTO_$WITH_COMPUTED_GOTO})

WITH_THREADING ?= 1
THREADING_0 = -DLIP_SINGLE_THREADED
THREADING_1 = -pthread
THREADING_FLAGS = $(eval echo \${THREADING_$WITH_THREADING})

WITH_COVERAGE ?= 0
COVERAGE_0 =
COVERAGE_1 = --coverage
COVERAGE_FLAGS = $(eval echo \${COVERAGE_$WITH_COVERAGE})

WITH_DBG_EMBED_RESOURCES ?= 0
DBG_EMBED_RESOURCES_0 = -DLIP_DBG_EMBED_RESOURCES=0
DBG_EMBED_RESOURCES_1 = -DLIP_DBG_EMBED_RESOURCES=1
DBG_FLAGS = $(eval echo \${DBG_EMBED_RESOURCES_$WITH_DBG_EMBED_RESOURCES})

OPTIMIZATION_0 = -O3
OPTIMIZATION_1 = -O0
OPTIMIZATION_FLAGS = $(eval echo \${OPTIMIZATION_$WITH_COVERAGE})

WITH_LTO ?= 1
LTO_0 =
LTO_1 = -flto
LTO_FLAGS = $(eval echo \${LTO_$WITH_LTO})

WITH_UBSAN ?= 0
UBSAN_COMPILE_FLAGS_0 =
UBSAN_COMPILE_FLAGS_1 = -fsanitize=undefined -fno-sanitize-recover=undefined
UBSAN_COMPILE_FLAGS = $(eval echo \${UBSAN_COMPILE_FLAGS_$WITH_UBSAN})
UBSAN_LINK_FLAGS_0 =
UBSAN_LINK_FLAGS_1 = -fsanitize=undefined -fno-sanitize-recover=undefined
UBSAN_LINK_FLAGS = $(eval echo \${UBSAN_LINK_FLAGS_$WITH_UBSAN})

WITH_ASAN ?= 0
ASAN_COMPILE_FLAGS_0 =
ASAN_COMPILE_FLAGS_1 = -fsanitize=address -DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=0
ASAN_COMPILE_FLAGS = $(eval echo \${ASAN_COMPILE_FLAGS_$WITH_ASAN})
ASAN_LINK_FLAGS_0 =
ASAN_LINK_FLAGS_1 = -fsanitize=address
ASAN_LINK_FLAGS = $(eval echo \${ASAN_LINK_FLAGS_$WITH_ASAN})

COMMON_FLAGS = ${THREADING_FLAGS} ${COVERAGE_FLAGS} ${OPTIMIZATION_FLAGS} ${LTO_FLAGS}
COMPILE_FLAGS = -g -Wall -Wextra -Werror -pedantic -Iinclude ${LIP_CONFIG_EXTRA_FLAGS} ${COMPUTED_GOTO_FLAGS} ${UBSAN_COMPILE_FLAGS} ${ASAN_COMPILE_FLAGS}
C_FLAGS ?= -std=c99 ${COMPILE_FLAGS} ${COMMON_FLAGS}
CPP_FLAGS ?= ${COMPILE_FLAGS} ${COMMON_FLAGS}
LINK_FLAGS ?= -g ${COMMON_FLAGS} ${UBSAN_LINK_FLAGS} ${ASAN_LINK_FLAGS}

-import cpp.nu
-include src/dbg-client

all: tests bin/lip doc ! live

doc: ! live
	doxygen

tests: repl-test unit-test ! live

unit-test: bin/tests ! live
	echo "-------------------------------------"
	bin/tests --color always

repl-test: src/tests/repl.sh bin/lip ! live
	${deps}

test:%: bin/tests ! live << SEED
	echo "-------------------------------------"
	bin/lip -v
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
		--filter '.*src/repl/.*' \
		--exclude '.*src/lip/vendor/.*' \
		--exclude '.*src/lip/khash_impl.c' \
		--html --html-details \
		--output $@/index.html

bin/tests: << C_FLAGS CPP_FLAGS CC LIBLIP LIBLIP_EXTRA_FLAGS CLEAR_ENV LINK_FLAGS
	${CLEAR_ENV}
	${NUMAKE} exe:$@ \
		sources="`find src/tests -name '*.cpp' -or -name '*.c'`" \
		c_flags="${C_FLAGS} -g ${LIBLIP_EXTRA_FLAGS} -Isrc" \
		link_flags="${LINK_FLAGS} -lm" \
		libs="${LIBLIP}"

bin/lip: << C_FLAGS CPP_FLAGS CC LIBLIP LIBLIP_EXTRA_FLAGS CLEAR_ENV
	${CLEAR_ENV}
	${NUMAKE} exe:$@ \
		sources="`find src/repl -name '*.cpp' -or -name '*.c'`" \
		c_flags="${C_FLAGS} ${LIBLIP_EXTRA_FLAGS} -Isrc/dbg -Ideps/linenoise -Ideps/cargo -Ideps/miniz" \
		linker="${CC}" \
		libs="bin/liblinenoise.a bin/libcargo.a bin/libminiz.a bin/libdbg.a bin/libcmp.a ${LIBLIP}"

bin/liblip.so: << CC C_FLAGS CLEAR_ENV LIP_CONFIG_H
	${CLEAR_ENV}
	c_flags="${C_FLAGS} -DLIP_DYNAMIC=1 -DLIP_BUILDING"
	${NUMAKE} --depend ${LIP_CONFIG_H}
	${NUMAKE} dynamic-lib:$@ \
		c_flags="${c_flags}" \
		linker="${CC}" \
		sources="$(find src/lip -name '*.cpp' -or -name '*.c')"

bin/liblip.a: << C_FLAGS CLEAR_ENV LIP_CONFIG_H
	${CLEAR_ENV}
	c_flags="${C_FLAGS} -DLIP_DYNAMIC=0"
	${NUMAKE} --depend ${LIP_CONFIG_H}
	${NUMAKE} static-lib:$@ \
		c_flags="${c_flags}" \
		sources="$(find src/lip -name '*.cpp' -or -name '*.c')"

include/lip/gen/%.h: src/lip/%.h.in << c_flags C_FLAGS link_flags LINK_FLAGS linker LINKER cc CC ar AR TRAVIS TRAVIS_COMMIT
	if test "${TRAVIS}" = "true"; then
		export LIP_VERSION="ci-${TRAVIS_COMMIT}"
	else
		export LIP_VERSION="$(git describe --tags)"
	fi
	export C_FLAGS="${c_flags:-${C_FLAGS}}"
	export LINK_FLAGS="${link_flags:-${LINK_FLAGS}}"
	export CC="${cc:-${CC}}"
	export AR="${ar:-${AR}}"
	export LINKER="${linker:-${LINKER}}"
	mkdir -p $(dirname $@)
	envsubst < ${deps} > $@

bin/libdbg.a: << CLEAR_ENV C_FLAGS DBG_FLAGS WITH_DBG_EMBED_RESOURCES
	${CLEAR_ENV}
	if [ "${WITH_DBG_EMBED_RESOURCES}" = 1 ]; then
		${NUMAKE} --depend src/dbg-client/gen
	fi
	${NUMAKE} static-lib:$@ \
		c_flags="${C_FLAGS} ${DBG_FLAGS} -Iinclude -Isrc/dbg-client/gen -Ideps/cmp -Wno-unused-function" \
		sources="$(find src/dbg -name '*.cpp' -or -name '*.c')"

bin/liblinenoise.a: << C_FLAGS CPP_FLAGS CLEAR_ENV ASAN_COMPILE_FLAGS UBSAN_COMPILE_FLAGS  LTO_FLAGS
	${CLEAR_ENV}
	${NUMAKE} static-lib:$@ \
		c_flags="-Ideps/linenoise ${ASAN_COMPILE_FLAGS} ${UBSAN_COMPILE_FLAGS} ${LTO_FLAGS}" \
		sources="`find deps/linenoise -name '*.cpp' -or -name '*.c'`"

bin/libcargo.a: << CLEAR_ENV ASAN_COMPILE_FLAGS UBSAN_COMPILE_FLAGS LTO_FLAGS
	${CLEAR_ENV}
	${NUMAKE} static-lib:$@ \
		c_flags="${LTO_FLAGS} ${ASAN_COMPILE_FLAGS} ${UBSAN_COMPILE_FLAGS}" \
		sources="deps/cargo/cargo.c"

bin/libcmp.a: << CLEAR_ENV ASAN_COMPILE_FLAGS UBSAN_COMPILE_FLAGS LTO_FLAGS
	${CLEAR_ENV}
	${NUMAKE} static-lib:$@ \
		c_flags="${LTO_FLAGS} ${ASAN_COMPILE_FLAGS} ${UBSAN_COMPILE_FLAGS}" \
		sources="deps/cmp/cmp.c"

bin/libminiz.a: << C_FLAGS CPP_FLAGS CLEAR_ENV ASAN_COMPILE_FLAGS UBSAN_COMPILE_FLAGS  LTO_FLAGS
	${CLEAR_ENV}
	${NUMAKE} static-lib:$@ \
		c_flags="${LTO_FLAGS} ${ASAN_COMPILE_FLAGS} ${UBSAN_COMPILE_FLAGS}" \
		sources="deps/miniz/miniz.c"

# Only for vm_dispatch.c, remove -pedantic because we will be using a
# non-standard extension (computed goto) if it is available
$BUILD_DIR/%/src/lip/vm_dispatch.c.ninja: << BUILD_DIR CPPNU_DIR
	BUILD_SUBDIR=${m}
	build_cfg="${BUILD_DIR}/${BUILD_SUBDIR}/.cfg"
	. ${build_cfg}
	c_flags=$(echo ${c_flags:-${C_FLAGS}} | sed 's/-pedantic//g')
	echo ${c_flags}
	${CPPNU_DIR}/compile c $@
	echo "  c_flags = ${c_flags}" >> $@
