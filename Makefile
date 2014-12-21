CFLAGS = -std=c99 -Wall -O0 -g3
LDLIBS = -ldl

all : main libgame.so

main : main.c

libgame.so : game.c
	$(CC) -shared -fPIC -o tmp_$@ $<
	mv tmp_$@ $@
	#$(CC) -shared -fPIC -o $@ $<

test : main libgame.so
	./$<

clean :
	$(RM) libgame.so main
