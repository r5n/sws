CFLAGS += -Wall -Werror -Wextra -pedantic -std=c99  -g

sws: args.o sws.o parse.o response.o
	$(CC) $(CFLAGS) -o $@ $^ $>

clean:
	rm *.o
	rm sws

format:
	clang-format -i *.[ch]

.PHONY: clean format
