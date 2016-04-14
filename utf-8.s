.intel_syntax noprefix;

.section .text
.global main

exit:
	mov     rax, 60    	 # exit()
	syscall

symbolvalue:
	mov	rax, QWORD PTR [rdi]
	shr	rax, 16
	ret

putchar:
	push	rdi
	mov	rdx, 0
plen:
	cmp	rdi, 0
	jz	pwrite
	shr	rdi, 8
	inc	rdx
	jmp	plen
pwrite:
	mov     rax, 1		# sys_write
	mov     rdi, 1		# stdout
	lea     rsi, [rsp]	# address, length in rdx
	syscall
	pop     rdi
	ret

getchar:
	mov	rdi, 0		# stdin
	push	rdi
	lea	rsi, [rsp]	# address
	mov	rdx, 1		# length
	mov	rax, 0		# sys_read
	syscall

	mov	rax, [rsp]	# byte read
	mov	rcx, 0
glen:
	shl	rax, 1
	and	rax, 0x01ff
	cmp	ah, 0
	jz	gread
	inc	cl
	jmp	glen
gread:
	mov	r8,  [rsp]	# accumulator
	cmp	cl,  0
	je	gdone
	dec	cl
	mov	r9,  rcx	# loop
	mov	r10, 0		# shifter
gmore:
	cmp	r9, 0
	jz	gdone
	mov	rdi, 0		# stdin
	lea	rsi, [rsp]	# address
	mov	rdx, 1		# length
	mov	rax, 0		# sys_read
	syscall
	mov	r11, [rsp]	# temp
	add	r10, 8		# shifter
	mov	rcx, r10
	shl	r11, cl
	add	r8,  r11
	dec	r9
	jmp	gmore
gdone:
	mov	rax, r8
	pop	rdi


	ret

main:
	mov     rdi, 0x61	# 'a'
	call    putchar

	mov     rdi, 0xac82e2	# '€'
	call    putchar

	call	getchar
	mov     rdi, rax
	call    putchar

	mov     rdi, 0x0a
	call    putchar

	mov     rdi, 0
	call    exit
