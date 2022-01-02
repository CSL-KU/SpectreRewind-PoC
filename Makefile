.PHONY: all clean

CFLAGS=-O2 -pthread
CC=gcc

BINS=detect_spr detect_spr_list
all: $(BINS)

clean:
	rm -rf $(BINS)
