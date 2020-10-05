CC = ../../gbdk/bin/lcc
CFLAGS = 

all:	clean rom

%.gb:
	$(CC) $(CFLAGS) -o $@ src/multitasking.c src/threads.c

clean:
	rm -rf multitasking.*

rom: multitasking.gb
