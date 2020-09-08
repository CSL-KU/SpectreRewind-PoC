.PHONY: all clean

all: detect_spr

detect_spr: detect_spr.c
	        gcc -O2 $< -o $@ 

clean:
	rm -rf detect_spr
