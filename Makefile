WARNING_FLAGS = -Wall
CFLAGS  = -std=c99 -pedantic $(WARNING_FLAGS) -O2 -fPIC

LDLIBS  = -ldl

all : tags main libgame.so libfun_pointer.so alternate_libfun_pointer.so

main : main.c game.h reload_patch.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

libgame.so : game.c game.h
	$(CC) $(CFLAGS) -shared $(LDFLAGS) -o $@ $< $(LDLIBS) -lncurses
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


