C_FLAGS ?= -Wall -Werror -pedantic
CPP_FLAGS ?= -Wall -Werror -pedantic

-import cpp.nu

all: bin/lip ! live

bin/lip: << C_FLAGS
	${NUMAKE} exe:$@ \
		sources="`find src -name '*.cpp' -or -name '*.c'`"
