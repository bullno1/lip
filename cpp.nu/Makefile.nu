BUILD_DIR ?= .build
CC ?= cc
CXX ?= c++
AR ?= ar
LINKER ?= ${CXX}
C_FLAGS ?= -Wall
CPP_FLAGS ?= -Wall
LINK_FLAGS ?=

GENERATE_CMD = $(readlink -f generate)

clean: << BUILD_DIR ! live
	find ${BUILD_DIR} -name 'build.*.ninja' \
		-exec ninja -f {} -t clean -r compile-cpp compile-c link \; || true
	${NUMAKE} --clean

# A phony rule to register target as an executable
exe:%: << BUILD_DIR GENERATE_CMD ! live
	${GENERATE_CMD} exe

# A phony rule to register target as a static library
static-lib:%: << BUILD_DIR GENERATE_CMD ! live
	${GENERATE_CMD} lib
	echo "-L$(dirname ${m}) -l:$(basename ${m})" > "${m}.meta"

# A phony rule to register target as a dynamic library
dynamic-lib:%: << BUILD_DIR GENERATE_CMD linker ! live
	${GENERATE_CMD} dll
	echo "-Wl,-rpath=\\\\$\$ORIGIN -L$(dirname ${m}) -l:$(basename ${m})" > "${m}.meta"

${BUILD_DIR}/%.build-cfg: << BUILD_DIR sources cc CC cxx CXX ar AR linker LINKER c_flags C_FLAGS cpp_flags CPP_FLAGS link_flags LINK_FLAGS CCC_ANALYZER_ANALYSIS CCC_ANALYZER_CONFIG CCC_ANALYZER_FORCE_ANALYZE_DEBUG_CODE CCC_ANALYZER_HTML CCC_ANALYZER_OUTPUT_FORMAT CCC_ANALYZER_PLUGINS CLANG CLANG_CXX
	export BUILD_SUBDIR="$(${NUMAKE} --hash)"
	mkdir -p ${BUILD_DIR}/$(dirname ${m})
	${NUMAKE} --env > $@

$BUILD_DIR/%/tmp_output: << BUILD_DIR BUILD_CMD
	BUILD_SUBDIR=${m%%/*}
	build_cfg="${BUILD_DIR}/${BUILD_SUBDIR}/.cfg"
	source ${build_cfg}
	build_script="${BUILD_DIR}/${BUILD_SUBDIR}/build.${BUILD_TYPE}.ninja"
	${NUMAKE} --depend ${build_cfg} ${build_script}
	ninja -f ${build_script}

BUILD_CMD = $(readlink -f build)
LINK_EXE = $(readlink -f link-exe.ninja)
LINK_LIB = $(readlink -f link-lib.ninja)
LINK_DLL = $(readlink -f link-dll.ninja)

$BUILD_DIR/%/build.exe.ninja: << BUILD_DIR BUILD_CMD LINK_EXE libs
	${BUILD_CMD} ${LINK_EXE} $@

$BUILD_DIR/%/build.lib.ninja: << BUILD_DIR BUILD_CMD LINK_LIB
	${BUILD_CMD} ${LINK_LIB} $@

$BUILD_DIR/%/build.dll.ninja: << BUILD_DIR BUILD_CMD LINK_DLL libs
	${BUILD_CMD} ${LINK_DLL} $@

COMPILE_CMD = $(readlink -f compile)

$BUILD_DIR/%.c.ninja: << BUILD_DIR COMPILE_CMD
	${COMPILE_CMD} c $@

$BUILD_DIR/%.cpp.ninja: << BUILD_DIR COMPILE_CMD
	${COMPILE_CMD} cpp $@
