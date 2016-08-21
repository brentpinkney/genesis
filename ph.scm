;;; -----------------------------------------------------------------------------------------------
;;; 	special-form	built-in	interpreted	assembler/code
;;; -----------------------------------------------------------------------------------------------
;;; !	set!
;;; ?	if
;;; ^	lambda
;;; $	fexpr
;;; @	code
;;; #			car
;;; %			cdr
;;; _			null?
;;; \			reverse
;;; .			cons
;;; =			equals
;;; `			apply
;;; :					memq
;;; ,					append
;;; '					quote
;;; ~					not
;;; |					or
;;; &					and
;;; +							add
;;; -							subtract
;;; *							multiply
;;; /							divide			*
;;; a					atom?	
;;; b					begin
;;; c					cond					*
;;; d			describe
;;; e			eval
;;; f					fold
;;; h							halt
;;; g					generate				*
;;; i			integer?
;;; k					list
;;; l					let
;;; m					map
;;; o			eval-list
;;; p			print
;;; q			assq
;;; r			read
;;; s			symbol?
;;; t			tuple?
;;; u							integer→symbol
;;; v					environment
;;; z			length

;;; -----------------------------------------------------------------------------------------------
;;; reflection → 
;;; -----------------------------------------------------------------------------------------------
;;; (v)	= (environment)
(!v($(EV)V))

;;; (aE) (atom? e)
(!a(^(E)(?E(?(tE)()E)())))

;;; (hN) (halt n)
(!h (@ 0x25                                                     ; function
       0x01							; n
       0x0f 0xb6 0x7e 0x02					; movzx  edi, BYTE PTR [rsi+0x2]
       0x49 0xba 0xff 0x00 0x00 0x00 0x00 0x00 0x00 0x00 	; movabs r10, 0xff = quit()
       0x41 0xff 0xd2						; call   r10
       0xc3))							; ret

;;; (uN) (utf-8 n) integer → symbol
(!u (@ 0x25							; function
       0x01							; n
       0x48 0x8b 0x5e 0x02					; mov    rbx, QWORD PTR [rsi+0x2]
       0x48 0x89 0xde						; mov    rsi, rbx
       0x49 0xba 0xf6 0x00 0x00 0x00 0x00 0x00 0x00 0x00	; movabs r10, 0xf6 = symbol()
       0x41 0xff 0xd2						; call   r10
       0xc3))							; ret

;;; -----------------------------------------------------------------------------------------------
;;; lists → 
;;; -----------------------------------------------------------------------------------------------

;;; (kEFGH…) (list e f g h …)
(!k($(EV)(oEV)))

;;; (,XY) (append X Y)
(!,(^(XY)(?X(.(#X)(,(%X)Y))Y)))

;;; (mFL) (map f l)
(!m(^(FL)(?L(.(F(#L))(mF(%L)))())))

;;; (fFXL) (fold f x l)
(!f(^(FXL)(?L(fF(F(#L)X)(%L))X)))

;;; (:XL) (memq? x l)
(!:(^(XL)(?L(?(=X(#L))L(:X(%L)))())))

;;; -----------------------------------------------------------------------------------------------
;;; lang →
;;; -----------------------------------------------------------------------------------------------

;;; ('E) (quote e)
(!'($(EV)(#E)))

;;; (bEFGH…) (begin e f g h …)
(!b($(EV)(e(k(,(k('^)())E)))))

;;; (l) and named let
(!l($(EV)
     (? (s(#E))
	(e(k(,(k('^)(k(#E)))(k(.(#E)(m#(m%(#(%E)))))))
	    (,(k('^)(m#(#(%E))))(%(%E)))))
	(e(.(,(k('^)(m#(#E)))(%E))(m#(m%(#E))))))))

;;; -----------------------------------------------------------------------------------------------
;;; boolean → 
;;; -----------------------------------------------------------------------------------------------

;;; (~E) (not e)
(!~(^(E)(?E()0x00)))

;;; (|XYZ…) (or z y z …)
(!| ($ (KV)
       (? K
	  (l L ((F (e(#K))) (R (%K)))
	     (? F
		F
		(? R
		   (L (e(#R)) (%R))
		   ())))
	  ())))

;;; (&XYZ…) (and z y z …)
(!& ($ (KV)
       (? K
	  (l L ((F (e(#K))) (R (%K)))
	     (? F
		(? R
		   (L (e(#R)) (%R))
		   F)
		()))
	  ())))

;;; -----------------------------------------------------------------------------------------------
;;; math →
;;; -----------------------------------------------------------------------------------------------
;;; (+ a b) 
(!+ (@ 0x25                                                     ; function
       0x02							; a, b
       0x48 0x8b 0x4e 0x02					; mov    rcx, QWORD PTR [rsi+0x2]
       0x48 0x8b 0x72 0x02					; mov    rsi, QWORD PTR [rdx+0x2]
       0x48 0x01 0xce						; add    rsi, rcx
       0x49 0xba 0xf5 0x00 0x00 0x00 0x00 0x00 0x00 0x00	; movabs r10, 0xf5 
       0x41 0xff 0xd2						; call   r10
       0xc3))							; ret

;;; (- a b) 
(!- (@ 0x25                                                     ; function
       0x02							; a, b
       0x48 0x8b 0x5e 0x02					; mov    rbx, QWORD PTR [rsi+0x2]
       0x48 0x8b 0x4a 0x02					; mov    rcx, QWORD PTR [rdx+0x2]
       0x48 0x29 0xcb						; sub    rbx, rcx
       0x48 0x89 0xde						; mov    rsi, rbx
       0x49 0xba 0xf5 0x00 0x00 0x00 0x00 0x00 0x00 0x00	; movabs r10, 0xf5 
       0x41 0xff 0xd2						; call   r10
       0xc3))							; ret

;;; (* a b) 
(!* (@ 0x25                                                     ; function
       0x02							; a, b
       0x48 0x8b 0x4e 0x02					; mov    rcx, QWORD PTR [rsi+0x2]
       0x48 0x8b 0x72 0x02					; mov    rsi, QWORD PTR [rdx+0x2]
       0x48 0x0f 0xaf 0xf1					; imul   rsi, rcx
       0x49 0xba 0xf5 0x00 0x00 0x00 0x00 0x00 0x00 0x00	; movabs r10, 0xf5
       0x41 0xff 0xd2						; call   r10
       0xc3))							; ret

;;; (FN) (factorial n)
(!F (^(N) (? (= N 0x00) 0x01 (* N (F (- N 0x01))))))
