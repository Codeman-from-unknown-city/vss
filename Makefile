CC      = gcc
CFLAGS  = -g3 -O0 # for debbuging
LDFLAGS = -lm

all: vssd

vssd: vssd.o cam.o net.o utils.o daemon.o

vssd.o: vssd.c net.h cam.h

cam.o: cam.c cam.h utils.h

net.o: net.c net.h utils.h

utils.o: utils.c utils.h

daemon.o: daemon.c daemon.h utils.h 

clean:
	rm *.o vssd

