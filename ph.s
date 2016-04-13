.intel_syntax noprefix;

halt:
	movzx	edi, BYTE PTR [rsi + 0x02]
	movabs	r10, 0xff		# quit()
	call	r10
	ret

add:
	mov	rcx, QWORD PTR [rsi + 0x02]
	mov	rsi, QWORD PTR [rdx + 0x02]
	add	rsi, rcx
	movabs	r10, 0xf5 		# integer()
	call	r10
	ret

subtract:
	mov	rbx, QWORD PTR [rsi + 0x02]
	mov	rcx, QWORD PTR [rdx + 0x02]
	sub	rbx, rcx
	mov	rsi, rbx
	movabs	r10, 0xf5 		# integer()
	call	r10
	ret

multiply:
	mov	rcx, QWORD PTR [rsi + 0x02]
	mov	rsi, QWORD PTR [rdx + 0x02]
	imul	rsi, rcx
	movabs	r10, 0xf5		# integer()
	call	r10
	ret
