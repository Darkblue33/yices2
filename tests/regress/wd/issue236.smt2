(set-logic QF_AUFBV)
(declare-const v0 Bool)
(declare-const v1 Bool)
(declare-const v2 Bool)
(declare-const v3 Bool)
(declare-const v4 Bool)
(declare-const v5 Bool)
(declare-const v6 Bool)
(declare-const v7 Bool)
(declare-const v8 Bool)
(declare-const v9 Bool)
(declare-const v10 Bool)
(declare-const v11 Bool)
(declare-const v12 Bool)
(declare-const v13 Bool)
(declare-const v14 Bool)
(declare-const v15 Bool)
(declare-const v16 Bool)
(declare-const v17 Bool)
(assert true)
(declare-const arr--5847150245411458408_-5847150245408210833-0 (Array (_ BitVec 9) (_ BitVec 12)))
(declare-const v18 Bool)
(declare-const bv_24-0 (_ BitVec 24))
(assert true)
(declare-sort S0 0)
(assert true)
(assert true)
(push 1)
(declare-sort S1 0)
(assert (distinct true true true true true (distinct (store arr--5847150245411458408_-5847150245408210833-0 (bvadd (_ bv276 9) (_ bv276 9)) #x2a4) arr--5847150245411458408_-5847150245408210833-0 arr--5847150245411458408_-5847150245408210833-0) true true true))
(reset-assertions)
(check-sat)
(assert (= (bvadd (_ bv276 9) (_ bv276 9)) (bvadd (_ bv276 9) (_ bv276 9))))
(reset-assertions)
(check-sat)
(declare-const v19 Bool)
(declare-const v20 Bool)
(push 1)
(declare-const v21 Bool)
(declare-const v22 Bool)
(declare-const bv_30-1 (_ BitVec 30))
(declare-const bv_26-0 (_ BitVec 26))
(assert v1)
