CC := gcc
CFLAGS := -Wall -Werror -ggdb

OBJECTS =
OBJECTS += udm.o

all: test

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: test.c $(OBJECTS)
	$(CC) $(CFLAGS) -ludev -o $@ $(OBJECTS) $<

clean:
	rm -f $(OBJECTS) test
