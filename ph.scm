;;; (v)	= (environment)
(!v($(EV)V))

;;; ('E) (quote e)
(!'($(EV)(#E)))

;;; (aE) (atom? e)
(!a(^(E)(?E(?(tE)()E)())))

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

;;; (bEFGH…) (begin e f g h …)
(!b($(EV)(e(k(,(k('^)())E)))))

;;; (l) and named let
(!l($(EV)
     (? (s(#E))
	(e(k(,(k('^)(k(#E)))(k(.(#E)(m#(m%(#(%E)))))))
	    (,(k('^)(m#(#(%E))))(%(%E)))))
	(e(.(,(k('^)(m#(#E)))(%E))(m#(m%(#E))))))))

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
