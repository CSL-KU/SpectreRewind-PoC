.PHONY: all clean

CFLAGS=-O2

all: detect_spr

detect_spr: detect_spr.c
	gcc -pthread $(CFLAGS) $< -o $@
	gcc -pthread $(CFLAGS) $< -S

clean:
	rm -rf detect_spr
