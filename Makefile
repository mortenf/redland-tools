#
# Makefile for redland-tools
#

# These settings may need to be changed.
CC = gcc

# The rest should not need to be changed.
CFLAGS = -g -Wall `redland-config --cflags`
LDFLAGS = `redland-config --libs` -lcrypto -lssl
#-Wl,--verbose

PROGS=rdf-smush rdf-tree rdf-load rdf-digest

all: $(PROGS)
	-

%: %.c
	$(CC) $< $(CFLAGS) $(LDFLAGS) -o $@

clean:
	rm -f *.o $(PROGS)

install: $(PROGS)
	cp -p $(PROGS) /usr/local/bin/
