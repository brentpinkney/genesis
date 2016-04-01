;;; pre-history

;;; ('E) (quote e)
(!'($(EV)(#E)))

;;; (kEFGH…) (list e f g h …)
(!k($(EV)(uEV)))

;;; (aXY) (append X Y)
(!a(^(XY)(?X(.(#X)(a(%X)Y))Y)))

;;; (mFL) (map f l)
(!m(^(FL)(?L(.(F(#L))(mF(%L)))())))

;;; (bEFGH…) (begin e f g h …)
;;; requires that the result is evaluated XXX

(begin e f g)
	((λ () e f g))

;;; (~E) (not e)
(!~(^(E)(?E()0x00)))

;;; (v)	= (environment)
(!v($(EV)V))

