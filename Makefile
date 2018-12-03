CFLAGS += -Wall -Werror -Wextra -pedantic -std=c99  -g

sws: args.o sws.o parse.o response.o cgi.o
	$(CC) $(CFLAGS) -o $@ $^ $>

clean:
	rm *.o
	rm sws

.PHONY: clean
