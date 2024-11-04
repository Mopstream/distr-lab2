CFLAGS=--std=c17 -Wall -pedantic -Isrc/ -ggdb -Wextra -Werror -DDEBUG
BUILDDIR=build
SRCDIR=pa2
CC=clang

# export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/home/mopstream/Documents/ITMO-4-course/distributed/pa2"

LD_PRELOAD=/home/mopstream/Documents/ITMO-4-course/distributed/pa2/libruntime.so ./pa2 -p 2 10 20

main:
	$(CC) -L. -lruntime -std=c99 -Wall -pedantic *.c -o pa2

debug:
	$(CC) -L. -lruntime -std=c99 -Wall -pedantic *.c -g -o jopa

build: clean
	mkdir -p $(BUILDDIR)
	

all: main

clean:
	rm -rf $(BUILDDIR)
	rm -rf events.log pipes.log