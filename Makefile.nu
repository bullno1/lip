C_FLAGS ?= -Wall -Werror -pedantic
CPP_FLAGS ?= -Wall -Werror -pedantic

-import cpp.nu

all: bin/test ! live

bin/test: bin/liblip.a << C_FLAGS CPP_FLAGS LINK_FLAGS
	${NUMAKE} exe:$@ \
		sources="`find src/test -name '*.cpp' -or -name '*.c'`" \
		c_flags="${C_FLAGS} -Isrc" \
		cpp_flags="${CPP_FLAGS} -Isrc" \
		link_flags="${LINK_FLAGS} -Lbin -llip"

bin/liblip.a:
	${NUMAKE} static-lib:$@ \
		sources="`find src/lip -name '*.cpp' -or -name '*.c'`"
