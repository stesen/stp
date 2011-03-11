CC=gcc
CFLAGS= -I/usr/include -I/usr/include/freetype2 -Wall -march=native -Os -pipe -fomit-frame-pointer
#CFLAGS = -I/usr/include -I/usr/include/freetype2 -Wall -g
LDFLAGS= -L/usr/lib -lX11 -lXft -lrt
PROGNAME=stp

$(PROGNAME): Makefile stp.c stp.h
	$(CC) $(CFLAGS) $(LDFLAGS) stp.c -o $(PROGNAME)
	strip -v $(PROGNAME)
	@ls -lh $(PROGNAME)

clean:
	rm -fr *.o $(PROGNAME)

.PHONY:clean $(PROGNAME)
