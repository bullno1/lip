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
	echo "-L$(dirname ${m}) -l:$(basename ${m})" > "${m}.meta"

# A phony rule to register target as a dynamic library
dynamic-lib:%: << BUILD_DIR cc CC c_flags C_FLAGS CCC_ANALYZER_ANALYSIS CCC_ANALYZER_CONFIG CCC_ANALYZER_FORCE_ANALYZE_DEBUG_CODE CCC_ANALYZER_HTML CCC_ANALYZER_OUTPUT_FORMAT CCC_ANALYZER_PLUGINS CLANG CLANG_CXX ! live
	BUILD_SUBDIR=$(${NUMAKE} --hash)
	tmp_output="${BUILD_DIR}/${BUILD_SUBDIR}/${m}.dll"
	${NUMAKE} --depend BUILD_SUBDIR=${BUILD_SUBDIR} ${tmp_output}
	mkdir -p $(dirname ${m})
	cp ${tmp_output} ${m}
	echo "-Wl,-rpath=\$ORIGIN -L$(dirname ${m}) -l:$(basename ${m})" > "${m}.meta"

# All target types require a similar compilation step
COMPILE_SOURCES = $(readlink -f compile-sources)

$BUILD_DIR/%.exe: << COMPILE_SOURCES sources BUILD_DIR BUILD_SUBDIR linker LINKER link_flags LINK_FLAGS libs
	${NUMAKE} --depend ${libs}
	mkdir -p $(dirname $@)
	${COMPILE_SOURCES} "$@.objs"
	objs=$(cat $@.objs)
	extra_flags=$(
		echo ${libs} | awk '{ for(i = 1; i <= NF; i++) { print $i ".meta"; } }' | xargs cat
	)
	echo ${linker:-${LINKER}} -o $@ ${link_flags:-${LINK_FLAGS}} ${objs} ${extra_flags}
	${linker:-${LINKER}} -o $@ ${link_flags:-${LINK_FLAGS}} ${objs} ${extra_flags}

$BUILD_DIR/%.lib: << COMPILE_SOURCES sources BUILD_DIR BUILD_SUBDIR ar AR
	mkdir -p $(dirname $@)
	${COMPILE_SOURCES} "$@.objs"
	objs=$(cat $@.objs)
	echo ${ar:-${AR}} rcs $@ ${objs}
	${ar:-${AR}} rcs $@ ${objs}

$BUILD_DIR/%.dll: << COMPILE_SOURCES sources BUILD_DIR BUILD_SUBDIR linker LINKER c_flags C_FLAGS link_flags LINK_FLAGS
	${NUMAKE} --depend ${libs}
	mkdir -p $(dirname $@)
	export c_flags="${c_flags:-${C_FLAGS}} -fPIC -fvisibility=hidden -ffunction-sections -fdata-sections"
	${COMPILE_SOURCES} "$@.objs"
	objs=$(cat $@.objs)
	mkdir -p $(dirname $@)
	echo ${linker:-${LINKER}} -fvisibility=hidden -ffunction-sections -fdata-sections -Wl,-gc-sections -shared -o $@ ${link_flags:-${LINK_FLAGS}} ${objs}
	${linker:-${LINKER}} -fvisibility=hidden -ffunction-sections -fdata-sections -Wl,-gc-sections --shared -o $@ ${link_flags:-${LINK_FLAGS}} ${objs}

# Compiling *.cpp and compiling *.c are pretty similar so we extract the common
# parts into a shell script

COMPILE = $(readlink -f compile)

$BUILD_DIR/%.c.o: << BUILD_SUBDIR COMPILE cc CC c_flags C_FLAGS CCC_ANALYZER_ANALYSIS CCC_ANALYZER_CONFIG CCC_ANALYZER_FORCE_ANALYZE_DEBUG_CODE CCC_ANALYZER_HTML CCC_ANALYZER_OUTPUT_FORMAT CCC_ANALYZER_PLUGINS CLANG CLANG_CXX
	source="${m#$BUILD_SUBDIR/}.c"
	${COMPILE} "${source}" "$@" "${cc:-${CC}}" "${c_flags:-${C_FLAGS}}"

$BUILD_DIR/%.cpp.o: << BUILD_SUBDIR COMPILE compile cxx CXX cpp_flags CPP_FLAGS CCC_ANALYZER_ANALYSIS CCC_ANALYZER_CONFIG CCC_ANALYZER_FORCE_ANALYZE_DEBUG_CODE CCC_ANALYZER_HTML CCC_ANALYZER_OUTPUT_FORMAT CCC_ANALYZER_PLUGINS CLANG CLANG_CXX
	source="${m#$BUILD_SUBDIR/}.cpp"
	${COMPILE} "${source}" "$@" "${cxx:-${CXX}}" "${cpp_flags:-${C_FLAGS}}"
