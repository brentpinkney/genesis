CFLAGS =-m64 -masm=intel -mcmodel=large -std=gnu99 -O0 -Wall -D_BSD_SOURCE
SFLAGS =-m64 -masm=intel -mcmodel=large

all: ph.o utf-8 ph-0 ph-1 ph-2 ph-3 ph-4 ph-5 ph-5-nostdlib ph-5-strings ph-6 

ph.o: ph.s 
	gcc $(SFLAGS) -c $^ -o $@

utf-8: utf-8.s 
	gcc $(SFLAGS) $^ -o $@

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
	gcc $(CFLAGS) $^ -fomit-frame-pointer -o $@

ph-5-nostdlib: ph-5-nostdlib.c
	gcc $(CFLAGS) $^ -fomit-frame-pointer -nostdlib -o $@
	strip -R .comment -R .note.gnu.build-id $@

ph-5-strings: ph-5-strings.c 
	gcc $(CFLAGS) $^ -g -fomit-frame-pointer -o $@

ph-6: ph-6.c 
	gcc $(CFLAGS) $^ -g -fomit-frame-pointer -o $@

clean:
	rm -f ph.o utf-8 ph-0 ph-1 ph-2 ph-3 ph-4 ph-5 ph-5-nostdlib ph-5-strings ph-6 
