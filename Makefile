CFLAGS  = -std=c99 -pedantic -Wall -O2 -fPIC
LDLIBS  = -ldl -lncurses

all : main libgame.so libfun_pointer.so alternate_libfun_pointer.so

main : main.c game.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

libgame.so : game.c game.h
	$(CC) $(CFLAGS) -shared $(LDFLAGS) -o $@ $< $(LDLIBS)
	make reload

libfun_pointer.so : fun_pointer.c game.h
	$(CC) $(CFLAGS) -shared $(LDFLAGS) -o $@ $< $(LDLIBS)
	make reload

alternate_libfun_pointer.so : alternate_fun_pointer.c game.h
	$(CC) $(CFLAGS) -shared $(LDFLAGS) -o alternate_libfun_pointer.so $< $(LDLIBS)

reload:
	pkill --signal USR1 main || true

test : main libgame.so
	./$<

clean :
	$(RM) *.o *.so main


