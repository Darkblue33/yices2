(set-logic QF_ABV)
(declare-const v0 Bool)
(declare-const v1 Bool)
(declare-const bv_11-0 (_ BitVec 11))
(declare-const v2 Bool)
(declare-const arr-2878601033469301130_2878601033469301130-0 (Array (_ BitVec 19) (_ BitVec 19)))
(assert true)
(push 1)
(assert true)
(assert true)
(declare-const arr-2878601033443320530_2497745055729741690-0 (Array (_ BitVec 11) (Array (_ BitVec 19) (_ BitVec 19))))
(assert (distinct bv_11-0 bv_11-0 (bvmul bv_11-0 bv_11-0) (_ bv0 11) (_ bv0 11)))
(declare-const v3 Bool)
(assert true)
(declare-const arr-2878601033469301130_2878601033440072955-0 (Array (_ BitVec 19) (_ BitVec 8)))
(assert true)
(declare-const bv_30-0 (_ BitVec 30))
(declare-const v4 Bool)
(declare-const arr-2878601033440072955_6020763715132213282-0 (Array (_ BitVec 8) (Array (_ BitVec 11) (Array (_ BitVec 19) (_ BitVec 19)))))
(pop 1)
(declare-const v5 Bool)
(assert true)
(reset-assertions)
(check-sat)
(push 1)
(assert (= arr-2878601033469301130_2878601033469301130-0 arr-2878601033469301130_2878601033469301130-0 arr-2878601033469301130_2878601033469301130-0 arr-2878601033469301130_2878601033469301130-0 arr-2878601033469301130_2878601033469301130-0))
