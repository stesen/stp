CFLAGS= -Wall -march=native -Os -pipe -fomit-frame-pointer

all:
	gcc $(CFLAGS) sysres.c -o sysres
	strip sysres
	@ls -lh sysres

clean:
	@rm -frv sysres

.PHONY:all clean
