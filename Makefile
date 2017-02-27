CFLAGS= -pedantic -Wall -Wextra -Wconversion -O3

all: search

search: search.c
	$(CC) $(CFLAGS) search.c -o search
