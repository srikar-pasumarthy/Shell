## Do not change this file
TARGETS=thsh parser_tester test_env

HEADERS=thsh.h
OBJECTS= parse.o builtin.o jobs.o history.o

CFLAGS= -Wall -Werror -g

.PHONY: all update clean

all: $(TARGETS)

%.o: %.c $(HEADERS)
	gcc $(CFLAGS) -c -o $@ $<

thsh: thsh.c $(OBJECTS) $(HEADERS)
	gcc $(CFLAGS) thsh.c $(OBJECTS) -o thsh

parser_tester: parser_tester.c $(OBJECTS) $(HEADERS)
	gcc $(CFLAGS) parser_tester.c $(OBJECTS) -o parser_tester

test_env: test_env.c $(OBJECTS) $(HEADERS)
	gcc $(CFLAGS) test_env.c $(OBJECTS) -o test_env

update:
	git pull https://github.com/comp530-f23/thsh.git lab2

clean:
	rm -f $(TARGETS) $(OBJECTS)
