all: vss

vss: main.o cam.o net.o
	$(CC) -o $@ $^ -lm

main.o: main.c net.h data.h cam.h
	$(CC) -c main.c

cam.o: cam.c cam.h data.h utils.h
	$(CC) -c cam.c

net.o: net.c net.h data.h utils.h
	$(CC) -c net.c

clean:
	rm *.o vss

