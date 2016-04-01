;;; pre-history

;;; (~E) (not e)
(!~(^(E)(?E()0x00)))

;;; (v)	= (environment)
(!v($(EV)V))

;;; ('E) (quote e)
(!'($(EV)(#E)))

;;; (kEFGH…) (list e f g h …)
(!k($(EV)(uEV)))

;;; (aXY) (append X Y)
(!a(^(XY)(?X(.(#X)(a(%X)Y))Y)))

;;; (mFL) (map f l)
(!m(^(FL)(?L(.(F(#L))(mF(%L)))())))

;;; (bEFGH…) (begin e f g h …)
(!b($(EV)(e(k(a(k('^)())E)))))

;;; (let)
(!l($(EV)(e(.(a(k('^)(m#(#E)))(%E))(m#(m%(#E)))))))

