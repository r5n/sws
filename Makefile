CFLAGS += -Wall -Werror -Wextra -pedantic -std=c99  -g
LDLIBS = -lm

sws: args.o sws.o parse.o response.o listing.o cgi.o
	$(CC) $(CFLAGS) $(LDLIBS) -o $@ $^ $>

clean:
	rm *.o
	rm sws

format:
	clang-format -i *.[ch]

.PHONY: clean format
