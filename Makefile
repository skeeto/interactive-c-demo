CFLAGS  = -std=c99 -Wall -O0 -g3 -fPIC
LDLIBS  = -ldl

all : main libgame.so

main : main.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

libgame.so : game.c
	$(CC) $(CFLAGS) -shared $(LDFLAGS) -o $@ $< $(LDLIBS)

test : main libgame.so
	./$<

clean :
	$(RM) *.o *.so main
