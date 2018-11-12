CFLAGS += -Wall -Werror -Wextra -pedantic -std=c99  -g
CC = cc
OBJS = extern.h

sws: args.o sws.c parse.c
	$(CC) $(CFLAGS) args.c parse.c sws.c -o sws

parse: parse.c
