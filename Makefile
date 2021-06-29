.PHONY: all clean

CFLAGS=-O2 -pthread
CC=gcc

BINS=detect_spr
all: $(BINS)

clean:
	rm -rf $(BINS)
