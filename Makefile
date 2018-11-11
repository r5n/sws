CFLAGS += -Wall -Werror -Wextra -pedantic -std=c99  -g
CC = cc
OBJS = extern.h

sws: args.o sws.c
	$(CC) $(CFLAGS) args.c sws.c -o sws

parse: parse.c
