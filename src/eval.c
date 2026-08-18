#include "eval.h"

#define eval_error(msg) (EvalResult){false, msg}
#define eval_ok() (EvalResult){true, NULL}

// Evaluate an expression and sets the value to out.
EvalResult evaluate(Expr *e, mpq_t out)
{
    if (!e) return eval_error("Empty expression");

    EvalResult res = {0};
    switch (e->kind)
    {
        case EXPR_ERROR:
            return eval_error("Invalid expression");

        case EXPR_NUMBER:
            mpq_set(out, e->as.number.value);
            break;

        case EXPR_INFIX:
            mpq_t l, r;
            mpq_inits(l, r, NULL);
            res = evaluate(e->as.infix.left, l);
            if (!res.ok)
            {
                mpq_clears(l, r, NULL);
                return res;
            }

            res = evaluate(e->as.infix.right, r);
            if (!res.ok)
            {
                mpq_clears(l, r, NULL);
                return res;
            }

            switch (e->as.infix.op)
            {
                case OP_ADD: mpq_add(out, l, r); break;
                case OP_SUB: mpq_sub(out, l, r); break;
                case OP_MUL: mpq_mul(out, l, r); break;

                case OP_DIV:
                    if (mpq_cmp_si(r, 0, 1) == 0)
                        return eval_error("Division by zero");
                    mpq_div(out, l, r);
                    break;

                default:
                     mpq_clears(l, r, NULL);
                     return eval_error("Unknown operator");
            }
            mpq_clears(l, r, NULL);
            break;

        case EXPR_PREFIX:
            res = evaluate(e->as.prefix.expr, out);
            if (!res.ok) return res;

            mpq_neg(out, out);
            break;
    }

    return eval_ok();
}
