CFLAGS =-m64 -masm=intel -mcmodel=large -std=gnu99 -g -O0 -Wall -D_BSD_SOURCE
SFLAGS =-m64 -masm=intel -mcmodel=large -c

all: ph-0 ph-1 ph-2 ph-3 ph-4 ph-5 ph-6 ph.o

ph-0: ph-0.c 
	gcc $(CFLAGS) $^ -o $@

ph-1: ph-1.c 
	gcc $(CFLAGS) $^ -o $@

ph-2: ph-2.c 
	gcc $(CFLAGS) $^ -o $@

ph-3: ph-3.c 
	gcc $(CFLAGS) $^ -o $@

ph-4: ph-4.c 
	gcc $(CFLAGS) $^ -o $@

ph-5: ph-5.c 
	gcc $(CFLAGS) $^ -o $@

ph-6: ph-6.c 
	gcc $(CFLAGS) $^ -o $@

ph.o: ph.S 
	gcc $(SFLAGS) $^ -o $@

clean:
	rm -f ph-0 ph-1 ph-2 ph-3 ph-4 ph-5 ph-6 ph.o
