BUILD_DIR ?= .build
CC ?= cc
CXX ?= c++
AR ?= ar
LINKER ?= ${CXX}
C_FLAGS ?= -Wall
CPP_FLAGS ?= -Wall
LINK_FLAGS ?=

# A phony rule to register target as an executable
exe:%: ${BUILD_DIR}/%.exe << BUILD_DIR ! live
	mkdir -p $(dirname ${m})
	cp ${deps} ${m}

# A phony rule to register target as a static library
static-lib:%: ${BUILD_DIR}/%.lib << BUILD_DIR ! live
	mkdir -p $(dirname ${m})
	cp ${deps} ${m}

# A temporary file will be built in the build directory.
# This is to avoid linking if object files are not changed.

$BUILD_DIR/%.exe: << sources BUILD_DIR linker LINKER link_flags LINK_FLAGS libs
	objs=$(
		echo ${sources} |
		awk -v BUILD_DIR=${BUILD_DIR} \
			'{ for(i = 1; i <= NF; i++) { print BUILD_DIR "/" $i ".o"; } }'
	)
	${NUMAKE} --depend ${objs} ${libs}
	mkdir -p $(dirname $@)
	echo ${linker:-${LINKER}} -o $@ ${link_flags:-${LINK_FLAGS}} ${objs} ${libs}
	${linker:-${LINKER}} -o $@ ${link_flags:-${LINK_FLAGS}} ${objs} ${libs}

$BUILD_DIR/%.lib: << sources BUILD_DIR ar AR
	objs=$(
		echo ${sources} |
		awk -v BUILD_DIR=${BUILD_DIR} \
			'{ for(i = 1; i <= NF; i++) { print BUILD_DIR "/" $i ".o"; } }'
	)
	${NUMAKE} --depend ${objs}
	mkdir -p $(dirname $@)
	echo ${ar:-${AR}} rcs $@ ${objs}
	${ar:-${AR}} rcs $@ ${objs}

# Compiling *.cpp and compiling *.c are pretty similar so we extract the common
# parts into a shell script

COMPILE = $(readlink -f compile.sh)

$BUILD_DIR/%.c.o: %.c << COMPILE cc CC c_flags C_FLAGS
	${NUMAKE} --depend ${COMPILE} # Compilation depends on the compile script too
	${COMPILE} "${deps}" "$@" "${cc:-${CC}}" "${c_flags:-${C_FLAGS}}"

$BUILD_DIR/%.cpp.o: %.cpp << COMPILE compile cxx CXX cpp_flags CPP_FLAGS
	${NUMAKE} --depend ${COMPILE}
	${COMPILE} "${deps}" "$@" "${cxx:-${CXX}}" "${cpp_flags:-${C_FLAGS}}"
