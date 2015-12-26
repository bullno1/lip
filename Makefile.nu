C_FLAGS ?= -Wall -Werror -pedantic
CPP_FLAGS ?= -Wall -Werror -pedantic

-import cpp.nu

all: tests ! live

tests: ! live
	for file in `find src/tests -name '*.c' -or -name '*.cpp'`
	do
		${NUMAKE} --depend test:$(basename ${file%%.*})
	done

test:%: ! live
	${NUMAKE} --depend exec:bin/tests/${m}

bin/tests/%: bin/liblip.a << C_FLAGS CPP_FLAGS LINK_FLAGS
	${NUMAKE} exe:$@ \
		sources="src/tests/${m}.c" \
		c_flags="${C_FLAGS} -Isrc" \
		cpp_flags="${CPP_FLAGS} -Isrc" \
		link_flags="${LINK_FLAGS} -Lbin -llip"

bin/liblip.a:
	${NUMAKE} static-lib:$@ \
		sources="`find src/lip -name '*.cpp' -or -name '*.c'`"

exec:%: % ! live
	echo "-------------------------------------"
	exec valgrind --leak-check=full ${m}
	echo "-------------------------------------"
