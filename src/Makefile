CC = gcc
CPPFLAGS =
CFLAGS = -std=c11 -D_XOPEN_SOURCE=700 -O2 -Wall -Wextra -Wformat=2
LDFLAGS =
LOADLIBES =
LDLIBS =

.DEFAULT: all
.PHONY: all zip 
all: tftpd

clean:
	rm -f *.o

distclean: clean
	rm -f tftpd

zip:
	cd ..; zip tftpd.zip README src/tftpd.c src/Makefile AUTHORS
