OBJECTS = pride-nyancat.o

CC	?=
CFLAGS	 ?= -g -Wall -Wextra -std=c99 -pedantic -Wwrite-strings
CPPFLAGS ?=
LDFLAGS  ?=

all: pride-nyancat

pride-nyancat: $(OBJECTS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@

clean:
	-rm -f $(OBJECTS) pride-nyancat

check: all
	# Unit tests go here. None currently.
	@echo "*** ALL TESTS PASSED ***"

.PHONY: all clean check
