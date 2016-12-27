BUILD_DIR ?= .build
CC ?= cc
CXX ?= c++
AR ?= ar
LINKER ?= ${CXX}
C_FLAGS ?= -Wall
CPP_FLAGS ?= -Wall
LINK_FLAGS ?=

# A phony rule to register target as an executable
exe:%: << BUILD_DIR cc CC c_flags C_FLAGS CCC_ANALYZER_ANALYSIS CCC_ANALYZER_CONFIG CCC_ANALYZER_FORCE_ANALYZE_DEBUG_CODE CCC_ANALYZER_HTML CCC_ANALYZER_OUTPUT_FORMAT CCC_ANALYZER_PLUGINS CLANG CLANG_CXX ! live
	BUILD_SUBDIR=$(${NUMAKE} --hash)
	tmp_output="${BUILD_DIR}/${BUILD_SUBDIR}/${m}.exe"
	${NUMAKE} --depend BUILD_SUBDIR=${BUILD_SUBDIR} ${tmp_output}
	mkdir -p $(dirname ${m})
	cp ${tmp_output} ${m}

# A phony rule to register target as a static library
static-lib:%: << BUILD_DIR cc CC c_flags C_FLAGS CCC_ANALYZER_ANALYSIS CCC_ANALYZER_CONFIG CCC_ANALYZER_FORCE_ANALYZE_DEBUG_CODE CCC_ANALYZER_HTML CCC_ANALYZER_OUTPUT_FORMAT CCC_ANALYZER_PLUGINS CLANG CLANG_CXX ! live
	BUILD_SUBDIR=$(${NUMAKE} --hash)
	tmp_output="${BUILD_DIR}/${BUILD_SUBDIR}/${m}.lib"
	${NUMAKE} --depend BUILD_SUBDIR=${BUILD_SUBDIR} ${tmp_output}
	mkdir -p $(dirname ${m})
	cp ${tmp_output} ${m}

# A temporary file will be built in the build directory.
# This is to avoid linking if object files are not changed.

$BUILD_DIR/%.exe: << sources BUILD_DIR BUILD_SUBDIR linker LINKER link_flags LINK_FLAGS libs
	objs=$(
		echo ${sources} |
		awk -v BUILD_DIR=${BUILD_DIR} -v BUILD_SUBDIR=${BUILD_SUBDIR} \
			'{ for(i = 1; i <= NF; i++) { print BUILD_DIR "/" BUILD_SUBDIR "/" $i ".o"; } }'
	)
	${NUMAKE} --depend ${objs} ${libs}
	mkdir -p $(dirname $@)
	echo ${linker:-${LINKER}} -o $@ ${link_flags:-${LINK_FLAGS}} ${objs} ${libs}
	${linker:-${LINKER}} -o $@ ${link_flags:-${LINK_FLAGS}} ${objs} ${libs}

$BUILD_DIR/%.lib: << sources BUILD_DIR BUILD_SUBDIR ar AR
	objs=$(
		echo ${sources} |
		awk -v BUILD_DIR=${BUILD_DIR} -v BUILD_SUBDIR=${BUILD_SUBDIR} \
			'{ for(i = 1; i <= NF; i++) { print BUILD_DIR "/" BUILD_SUBDIR "/" $i ".o"; } }'
	)
	${NUMAKE} --depend ${objs}
	mkdir -p $(dirname $@)
	echo ${ar:-${AR}} rcs $@ ${objs}
	${ar:-${AR}} rcs $@ ${objs}

# Compiling *.cpp and compiling *.c are pretty similar so we extract the common
# parts into a shell script

COMPILE = $(readlink -f compile)

$BUILD_DIR/%.c.o: << BUILD_SUBDIR COMPILE cc CC c_flags C_FLAGS CCC_ANALYZER_ANALYSIS CCC_ANALYZER_CONFIG CCC_ANALYZER_FORCE_ANALYZE_DEBUG_CODE CCC_ANALYZER_HTML CCC_ANALYZER_OUTPUT_FORMAT CCC_ANALYZER_PLUGINS CLANG CLANG_CXX
	source="${m#$BUILD_SUBDIR/}.c"
	${NUMAKE} --depend ${COMPILE} ${source} # Compilation depends on the compile script too
	${COMPILE} "${source}" "$@" "${cc:-${CC}}" "${c_flags:-${C_FLAGS}}"

$BUILD_DIR/%.cpp.o: << BUILD_SUBDIR COMPILE compile cxx CXX cpp_flags CPP_FLAGS CCC_ANALYZER_ANALYSIS CCC_ANALYZER_CONFIG CCC_ANALYZER_FORCE_ANALYZE_DEBUG_CODE CCC_ANALYZER_HTML CCC_ANALYZER_OUTPUT_FORMAT CCC_ANALYZER_PLUGINS CLANG CLANG_CXX
	source="${m#$BUILD_SUBDIR/}.cpp"
	${NUMAKE} --depend ${COMPILE} ${source} # Compilation depends on the compile script too
	${COMPILE} "${source}" "$@" "${cxx:-${CXX}}" "${cpp_flags:-${C_FLAGS}}"
