#
# Makefile
#

CC = gcc -c
LD = gcc
DEBUG = -g -Wall

%.o: %.c
	$(CC) $(DEBUG) -o $*.o $< 

all: test

client: client.o
	$(LD) -o client $^

test: core.o config.o log.o hash.o device.o event.o app.o aquasent.o
	$(LD) -o test $^

clean:
	rm *.o
	rm test
