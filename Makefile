CFLAGS  = -std=c99 -Wall -O3 -g3 -fPIC
LDLIBS  = -ldl -lncurses

all : main libgame.so

main : main.c game.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

libgame.so : game.c game.h
	$(CC) $(CFLAGS) -shared $(LDFLAGS) -o $@ $< $(LDLIBS)

test : main libgame.so
	./$<

clean :
	$(RM) *.o *.so main
