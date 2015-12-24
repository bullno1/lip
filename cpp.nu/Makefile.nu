BUILD_DIR ?= .build
C_COMPILER ?= gcc
CPP_COMPILER ?= g++
LINKER ?= g++
C_FLAGS ?= -Wall
CPP_FLAGS ?= -Wall

# A phony rule to register target as an executable
exe:%: ${BUILD_DIR}/exe-% << BUILD_DIR ! live
	${NUMAKE} --order-only dir:$(dirname ${m})
	cp ${BUILD_DIR}/exe-${m} $m

# A temporary file will be built in the build directory.
# This is to avoid linking if object files are not changed.
$BUILD_DIR/exe-%: << sources BUILD_DIR linker LINKER
	objs=
	for file in ${sources}
	do
		obj_file="${BUILD_DIR}/${file}.o"
		objs="${objs} ${obj_file}"
	done
	${NUMAKE} --depend ${objs}
	${NUMAKE} --order-only dir:$(dirname $@)

	${linker:-${LINKER}} -o $@ ${objs}

# Compiling *.cpp and compiling *.c are pretty similar so we extract the common
# parts into a shell script

COMPILE = $(readlink -f compile.sh)

$BUILD_DIR/%.c.o: %.c << COMPILE c_compiler C_COMPILER c_flags C_FLAGS
	${NUMAKE} --depend ${COMPILE} # Compilation depends on the compile script too
	${COMPILE} "${deps}" "$@" "${c_compiler:-${C_COMPILER}}" "${c_flags:-${C_FLAGS}}"

$BUILD_DIR/%.cpp.o: %.cpp << COMPILE compile cpp_compiler CPP_COMPILER cpp_flags CPP_FLAGS
	${NUMAKE} --depend ${COMPILE}
	${COMPILE} "${deps}" "$@" "${cpp_compiler:-${CPP_COMPILER}}" "${cpp_flags:-${C_FLAGS}}"

dir:%: ! live
	mkdir -p ${m}
