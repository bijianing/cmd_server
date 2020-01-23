cmd_srv: server.c
	gcc -o cmd_srv server.c

clean:
	@-rm -f cmd_srv

.PHONY: clean
