CC      = gcc
CFLAGS  = -g3 -O0 # for debbuging
LDFLAGS = -lm

all: vss

vss: vss.o cam.o net.o utils.o

vss.o: vss.c net.h cam.h

cam.o: cam.c cam.h utils.h

net.o: net.c net.h utils.h

utils.o: utils.c utils.h

clean:
	rm *.o vss

