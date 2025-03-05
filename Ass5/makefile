CC = gcc
CFLAGS = -Wall
LDFLAGS = -lm

all: leader follower

leader: leader.c
	$(CC) $(CFLAGS) -o leader leader.c $(LDFLAGS)

follower: follower.c
	$(CC) $(CFLAGS) -o follower follower.c $(LDFLAGS) -fopenmp

clean:
	rm -f leader follower *.o
