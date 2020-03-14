SRC_FILES=main.c utils.c

SRC_FILES:=$(addprefix src/,$(SRC_FILES))

CFLAGS=-lconfig -Iinclude

cmd_srv: $(SRC_FILES)
	gcc -o $@ $^ $(CFLAGS)

clean:
	@-rm -f cmd_srv

.PHONY: clean
