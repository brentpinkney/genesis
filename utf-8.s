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

	mov	rbx, [rsp]	# byte read
	mov	rcx, 1
glen:
	shl	rbx, cl
	and	rbx, 0x01ff
	cmp	bh, 0
	jz	gread
	inc	cl
	jmp	glen
gread:
	mov	rbx, [rsp]	# reload first byte
	cmp	cl,  1
	je	gdone
	dec	cl
	mov	r15, rcx
	mov	r14, 0		# cl to shl, but rcx clobbered
gmore:
	cmp	r15, 0
	jz	gdone
	mov	rdi, 0		# stdin
	lea	rsi, [rsp]	# address
	mov	rdx, 1		# length
	mov	rax, 0		# sys_read
	syscall
	mov	rbp, [rsp]	# temp
	add	r14, 8
	mov	rcx, r14
	shl	rbp, cl
	add	rbx, rbp
	dec	r15
	jmp	gmore
gdone:
	mov	rax, rbx
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
