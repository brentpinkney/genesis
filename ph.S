.intel_syntax noprefix;

halt:
	mov    rdi,QWORD PTR [rsi]
	and    edi,0xff00
	shr    rdi,0x8
	movabs r10,0xff			// = quit()
	call   r10
	ret

add:
	mov    rsi,QWORD PTR [rsi]
	and    rsi,0xff00
	shr    rsi,0x8
	mov    rdx,QWORD PTR [rdx]
	and    rdx,0xff00
	shr    rdx,0x8
	add    rsi,rdx
	movabs r10,0xfb 		// = integer()
	call   r10
	ret

subtract:
	mov    rsi,QWORD PTR [rsi]
	and    rsi,0xff00
	shr    rsi,0x8
	mov    rdx,QWORD PTR [rdx]
	and    rdx,0xff00
	shr    rdx,0x8
	sub    rsi,rdx
	movabs r10,0xfb			// = integer()
	call   r10
	ret

multiply:
	mov    rsi,QWORD PTR [rsi]
	and    rsi,0xff00
	shr    rsi,0x8
	mov    rdx,QWORD PTR [rdx]
	and    rdx,0xff00
	shr    rdx,0x8
	imul   rsi,rdx
	movabs r10,0xfb			// = integer()
	call   r10
	ret

