SRC_FILES=main.c cmds.c utils.c
SRC_CONFTEST=conftest.c utils.c

SRC_FILES:=$(addprefix src/,$(SRC_FILES))
SRC_CONFTEST:=$(addprefix src/,$(SRC_CONFTEST))

CFLAGS=-lconfig -Iinclude


cmd_srv: $(SRC_FILES)
	gcc -o $@ $^ $(CFLAGS)

conftest: $(SRC_CONFTEST)
	gcc -o $@ $^ $(CFLAGS)

clean:
	@-rm -f cmd_srv conftest

.PHONY: clean
