BUILD_DIR ?= .build
CC ?= cc
CXX ?= c++
AR ?= ar
LINKER ?= ${CXX}
C_FLAGS ?= -Wall
CPP_FLAGS ?= -Wall
LINK_FLAGS ?=

CLEAR_ENV = . $(readlink -f clear-env)

CPPNU_DIR = $(readlink -f .)

setup-ycm: ${BUILD_DIR}/compile_commands.json .ycm_extra_conf.py << BUILD_DIR ! live

clean: << BUILD_DIR ! live
	find ${BUILD_DIR} -name 'build.*.ninja' \
		-exec ninja -f {} -t clean -r compile-cpp compile-c link \; || true
	${NUMAKE} --clean

${BUILD_DIR}/compile_commands.json: << BUILD_DIR CPPNU_DIR ! live
	subdbs=$(
		find ${BUILD_DIR} -name '*.build-cfg' | \
		  sed 's/.build-cfg$/.compdb/g'
	)
	${NUMAKE} --depend ${subdbs} ${CPPNU_DIR}/combine-compdb
	mkdir -p $(dirname $@)
	${CPPNU_DIR}/combine-compdb ${subdbs} > $@

.ycm_extra_conf.py: ${CPPNU_DIR}/ycm_extra_conf.py << CPPNU_DIR
	cp ${deps} $@

# A phony rule to register target as an executable
exe:%: << BUILD_DIR CPPNU_DIR ! live
	${CPPNU_DIR}/generate exe

# A phony rule to register target as a static library
static-lib:%: << BUILD_DIR CPPNU_DIR ! live
	${CPPNU_DIR}/generate lib
	echo "${m}" > "${m}.meta"

# A phony rule to register target as a dynamic library
dynamic-lib:%: << BUILD_DIR CPPNU_DIR ! live
	${CPPNU_DIR}/generate dll
	echo "-Wl,-rpath=\\'\$\$ORIGIN\\' -L$(dirname ${m}) -l:$(basename ${m})" > "${m}.meta"

${BUILD_DIR}/%.compdb: << BUILD_DIR
	. ${BUILD_DIR}/${m}.build-cfg
	${NUMAKE} --depend ${BUILD_DIR}/${BUILD_SUBDIR}/.cfg
	. ${BUILD_DIR}/${BUILD_SUBDIR}/.cfg
	ninja \
		-f ${BUILD_DIR}/${BUILD_SUBDIR}/build.${BUILD_TYPE}.ninja \
		-t compdb compile-c compile-cpp > $@

${BUILD_DIR}/%.build-cfg: << BUILD_DIR sources libs cc CC cxx CXX ar AR linker LINKER c_flags C_FLAGS cpp_flags CPP_FLAGS link_flags LINK_FLAGS CCC_ANALYZER_ANALYSIS CCC_ANALYZER_CONFIG CCC_ANALYZER_FORCE_ANALYZE_DEBUG_CODE CCC_ANALYZER_HTML CCC_ANALYZER_OUTPUT_FORMAT CCC_ANALYZER_PLUGINS CLANG CLANG_CXX
	export BUILD_SUBDIR="$(${NUMAKE} --hash)"
	mkdir -p ${BUILD_DIR}/$(dirname ${m})
	${NUMAKE} --env | tee $@

$BUILD_DIR/%/build.exe.ninja: << BUILD_DIR CPPNU_DIR libs
	${CPPNU_DIR}/build ${CPPNU_DIR}/link-exe.ninja $@

$BUILD_DIR/%/build.lib.ninja: << BUILD_DIR CPPNU_DIR
	${CPPNU_DIR}/build ${CPPNU_DIR}/link-lib.ninja $@

$BUILD_DIR/%/build.dll.ninja: << BUILD_DIR CPPNU_DIR libs
	${CPPNU_DIR}/build ${CPPNU_DIR}/link-dll.ninja $@

$BUILD_DIR/%.c.ninja: << BUILD_DIR CPPNU_DIR
	${CPPNU_DIR}/compile c $@

$BUILD_DIR/%.cpp.ninja: << BUILD_DIR CPPNU_DIR
	${CPPNU_DIR}/compile cpp $@
