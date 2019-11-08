CC = gcc
LD = gcc
CFLAGS = -Wall -O0 `mysql_config --cflags --libs`
SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all
all: notes_tracker

notes_tracker: $(OBJECTS)
	$(CC) $(OBJECTS) $(CFLAGS) -o notes_tracker

%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf *.o notes_tracker
	rm -rf bundle.tgz

.PHONY: rebuild
rebuild: clean all

.PHONY: bundle
bundle:
	rm -rf bundle.tgz
	tar -cvzf bundle.tgz Makefile notes_tracker.c shellinaboxd.service
