/*
 *      CRF1d context (forward-backward, viterbi, etc).
 *
 * Copyright (c) 2007-2010, Naoaki Okazaki
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of the authors nor the names of its contributors
 *       may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $Id$ */

#ifdef    HAVE_CONFIG_H
#include <config.h>
#endif/*HAVE_CONFIG_H*/

#include <os.h>

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <crfsuite.h>

#include "crf1d.h"
#include "vecmath.h"



crf1d_context_t::crf1d_context_t(int flag, int L, int T)
{
    int ret = 0;

    this->flag = flag;
    this->num_labels = L;

    this->trans = std::vector<floatval_t>(L*L);

    if (this->flag & CTXF_MARGINALS) {
        this->exp_trans = std::vector<floatval_t>(L*L);
        this->mexp_trans = std::vector<floatval_t>(L*L);
    }

    crf1dc_set_num_items(T);
    /* T gives the 'hint' for maximum length of items. */
    this->num_items = 0;
}

void crf1d_context_t::crf1dc_set_num_items(int T)
{
    const int L = this->num_labels;

    this->num_items = T;

    if (this->cap_items < T) {

        this->alpha_score = std::vector<floatval_t>(T*L);
        this->beta_score = std::vector<floatval_t>(T*L);
        this->scale_factor = std::vector<floatval_t>(T);
        this->row = std::vector<floatval_t>(L);

        if (this->flag & CTXF_VITERBI) {
            this->backward_edge = std::vector<int>(T*L);
        }

        this->state = std::vector<floatval_t>(T*L);

        if (this->flag & CTXF_MARGINALS) {
            this->exp_state = std::vector<floatval_t>(T*L);
            this->mexp_state = std::vector<floatval_t>(T*L);
        }

        this->cap_items = T;
    }
}

void crf1d_context_t::crf1dc_reset(int flag)
{
    const int T = this->num_items;
    const int L = this->num_labels;

    if (flag & RF_STATE) {
        for (auto &x: this->state)
            x = 0.0;
    }
    if (flag & RF_TRANS) {
        std::fill_n(this->trans.begin(), L*L, 0.0);
    }

    if (this->flag & CTXF_MARGINALS) {
        std::fill_n(this->mexp_state.begin(), T*L, 0.0);
        std::fill_n(this->mexp_trans.begin(), L*L, 0.0);
        this->log_norm = 0;
    }
}

void crf1d_context_t::crf1dc_exp_state()
{
    const int T = this->num_items;
    const int L = this->num_labels;

    for (auto i = 0; i < T*L; ++i) {
        this->exp_state[i] = exp(this->state[i]);
    }
}

void crf1d_context_t::crf1dc_exp_transition()
{
    const int L = this->num_labels;

    std::copy_n(this->trans.begin(), L*L, this->exp_trans.begin());
    vecexp(this->exp_trans.begin(), L * L);
}

void crf1d_context_t::crf1dc_alpha_score()
{
    int i, t;
    floatval_t sum, *cur = NULL;
    floatval_t *scale = &this->scale_factor[0];
    const floatval_t *prev = NULL, *trans = NULL, *state = NULL;
    const int T = this->num_items;
    const int L = this->num_labels;

    /* Compute the alpha scores on nodes (0, *).
        alpha[0][j] = state[0][j]
     */
    cur = ALPHA_SCORE(this, 0);
    state = EXP_STATE_SCORE(this, 0);
    std::copy_n(state, L, cur);
    sum = vecsum(cur, L);
    *scale = (sum != 0.) ? 1. / sum : 1.;
    vecscale(cur, *scale, L);
    ++scale;

    /* Compute the alpha scores on nodes (t, *).
        alpha[t][j] = state[t][j] * \sum_{i} alpha[t-1][i] * trans[i][j]
     */
    for (t = 1;t < T;++t) {
        prev = ALPHA_SCORE(this, t-1);
        cur = ALPHA_SCORE(this, t);
        state = EXP_STATE_SCORE(this, t);

        std::fill_n(cur, L, 0.0);
        for (i = 0;i < L;++i) {
            trans = EXP_TRANS_SCORE(this, i);
            vecaadd(cur, prev[i], trans, L);
        }
        vecmul(cur, state, L);
        sum = vecsum(cur, L);
        *scale = (sum != 0.) ? 1. / sum : 1.;
        vecscale(cur, *scale, L);
        ++scale;
    }

    /* Compute the logarithm of the normalization factor here.
        norm = 1. / (C[0] * C[1] ... * C[T-1])
        log(norm) = - \sum_{t = 0}^{T-1} log(C[t]).
     */
    this->log_norm = -vecsumlog(this->scale_factor.begin(), T);
}

void crf1d_context_t::crf1dc_beta_score()
{
    int i, t;
    floatval_t *cur = NULL;
    const floatval_t *next = NULL, *state = NULL, *trans = NULL;
    const int T = this->num_items;
    const int L = this->num_labels;
    const floatval_t *scale = &this->scale_factor[T-1];

    /* Compute the beta scores at (T-1, *). */
    cur = BETA_SCORE(this, T-1);
    vecset(cur, *scale, L);
    --scale;

    /* Compute the beta scores at (t, *). */
    for (t = T-2;0 <= t;--t) {
        cur = BETA_SCORE(this, t);
        next = BETA_SCORE(this, t+1);
        state = EXP_STATE_SCORE(this, t+1);

        std::copy_n(next, L, this->row.begin());
        vecmul(this->row.begin(), state, L);

        /* Compute the beta score at (t, i). */
        for (i = 0;i < L;++i) {
            trans = EXP_TRANS_SCORE(this, i);
            cur[i] = vecdot(trans, this->row.begin(), L);
        }
        vecscale(cur, *scale, L);
        --scale;
    }
}

void crf1d_context_t::crf1dc_marginals()
{
    int i, j, t;
    const int T = this->num_items;
    const int L = this->num_labels;

    /*
        Compute the model expectations of states.
            p(t,i) = fwd[t][i] * bwd[t][i] / norm
                   = (1. / C[t]) * fwd'[t][i] * bwd'[t][i]
     */
    for (t = 0;t < T;++t) {
        floatval_t *fwd = ALPHA_SCORE(this, t);
        floatval_t *bwd = BETA_SCORE(this, t);
        floatval_t *prob = STATE_MEXP(this, t);
        std::copy_n(fwd, L, prob);
        vecmul(prob, bwd, L);
        vecscale(prob, 1. / this->scale_factor[t], L);
    }

    /*
        Compute the model expectations of transitions.
            p(t,i,t+1,j)
                = fwd[t][i] * edge[i][j] * state[t+1][j] * bwd[t+1][j] / norm
                = (fwd'[t][i] / (C[0] ... C[t])) * edge[i][j] * state[t+1][j] * (bwd'[t+1][j] / (C[t+1] ... C[T-1])) * (C[0] * ... * C[T-1])
                = fwd'[t][i] * edge[i][j] * state[t+1][j] * bwd'[t+1][j]
        The model expectation of a transition (i -> j) is the sum of the marginal
        probabilities p(t,i,t+1,j) over t.
     */
    for (t = 0;t < T-1;++t) {
        floatval_t *fwd = ALPHA_SCORE(this, t);
        floatval_t *state = EXP_STATE_SCORE(this, t+1);
        floatval_t *bwd = BETA_SCORE(this, t+1);

        /* row[j] = state[t+1][j] * bwd'[t+1][j] */
        std::copy_n(bwd, L, this->row.begin());
        vecmul(this->row.begin(), state, L);

        for (i = 0;i < L;++i) {
            floatval_t *edge = EXP_TRANS_SCORE(this, i);
            floatval_t *prob = TRANS_MEXP(this, i);
            for (j = 0;j < L;++j) {
                prob[j] += fwd[i] * edge[j] * row[j];
            }
        }
    }
}

floatval_t crf1d_context_t::crf1dc_marginal_point(int l, int t)
{
    floatval_t *fwd = ALPHA_SCORE(this, t);
    floatval_t *bwd = BETA_SCORE(this, t);
    return fwd[l] * bwd[l] / this->scale_factor[t];
}

floatval_t crf1d_context_t::crf1dc_marginal_path(const int *path, int begin, int end)
{
    int t;
    /*
        Compute the marginal probability of a (partial) path.
            a = path[begin], b = path[begin+1], ..., y = path[end-2], z = path[end-1]
            fwd[begin][a] = (fwd'[begin][a] / (C[0] ... C[begin])
            bwd[end-1][z] = (bwd'[end-1][z] / (C[end-1] ... C[T-1]))
            norm = 1 / (C[0] * ... * C[T-1])
            p(a, b, ..., z)
                = fwd[begin][a] * edge[a][b] * state[begin+1][b] * ... * edge[y][z] * state[end-1][z] * bwd[end-1][z] / norm
                = fwd'[begin][a] * edge[a][b] * state[begin+1][b] * ... * edge[y][z] * state[end-1][z] * bwd'[end-1][z] * (C[begin+1] * ... * C[end-2])
     */
    floatval_t *fwd = ALPHA_SCORE(this, begin);
    floatval_t *bwd = BETA_SCORE(this, end-1);
    floatval_t prob = fwd[path[begin]] * bwd[path[end-1]] / this->scale_factor[begin];

    for (t = begin;t < end-1;++t) {
        floatval_t *state = EXP_STATE_SCORE(this, t+1);
        floatval_t *edge = EXP_TRANS_SCORE(this, path[t]);
        prob *= (edge[path[t+1]] * state[path[t+1]] * this->scale_factor[t]);
    }

    return prob;
}

#if 0
/* Sigh, this was found to be slower than the forward-backward algorithm. */

#define    ADJACENCY(ctx, i) \
    (&MATRIX(this->adj, this->num_labels, 0, i))

void crf1dc_marginal_without_beta()
{
    int i, j, t;
    floatval_t *prob = NULL;
    floatval_t *row = this->row;
    const floatval_t *fwd = NULL;
    const int T = this->num_items;
    const int L = this->num_labels;

    /*
        Compute marginal probabilities of states at T-1
            p(T-1,j) = fwd'[T-1][j]
     */
    fwd = ALPHA_SCORE(this, T-1);
    prob = STATE_MEXP(this, T-1);
    veccopy(prob, fwd, L);

    /*
        Repeat the following computation for t = T-1,T-2, ..., 1.
            1) Compute p(t-1,i,t,j) using p(t,j)
            2) Compute p(t,i) using p(t-1,i,t,j)
     */
    for (t = T-1;0 < t;--t) {
        fwd = ALPHA_SCORE(this, t-1);
        prob = STATE_MEXP(this, t);

        veczero(this->adj, L*L);
        veczero(row, L);

        /*
            Compute adj[i][j] and row[j].
                adj[i][j] = fwd'[t-1][i] * edge[i][j]
                row[j] = \sum_{i} adj[i][j]
         */
        for (i = 0;i < L;++i) {
            floatval_t *adj = ADJACENCY(this, i);
            floatval_t *edge = EXP_TRANS_SCORE(this, i);
            vecaadd(adj, fwd[i], edge, L);
            vecadd(row, adj, L);
        }

        /*
            Find z such that z * \sum_{i] adj[i][j] = p(t,j).
            Thus, z = p(t,j) / row[j]; we overwrite row with z.
         */
        vecinv(row, L);
        vecmul(row, prob, L);

        /*
            Apply the partition factor z (row[j]) to adj[i][j].
         */
        for (i = 0;i < L;++i) {
            floatval_t *adj = ADJACENCY(this, i);
            vecmul(adj, row, L);
        }

        /*
            Now that adj[i][j] presents p(t-1,i,t,j),
            accumulate model expectations of transitions.
         */
        for (i = 0;i < L;++i) {
            floatval_t *adj = ADJACENCY(this, i);
            floatval_t *prob = TRANS_MEXP(this, i);
            vecadd(prob, adj, L);
        }

        /*
            Compute the marginal probability of states at t-1.
                p(t-1,i) = \sum_{j} p(t-1,i,t,j)
         */
        prob = STATE_MEXP(this, t-1);
        for (i = 0;i < L;++i) {
            floatval_t *adj = ADJACENCY(this, i);
            prob[i] = vecsum(adj, L);
        }
    }
}
#endif

floatval_t crf1d_context_t::crf1dc_score(const std::vector<int>& labels)
{
    int i, j, t;
    floatval_t ret = 0;
    const floatval_t *state = NULL, *cur = NULL, *trans = NULL;
    const int T = this->num_items;
    const int L = this->num_labels;

    /* Stay at (0, labels[0]). */
    i = labels[0];
    state = STATE_SCORE(this, 0);
    ret = state[i];

    /* Loop over the rest of items. */
    for (t = 1;t < T;++t) {
        j = labels[t];
        trans = TRANS_SCORE(this, i);
        state = STATE_SCORE(this, t);

        /* Transit from (t-1, i) to (t, j). */
        ret += trans[j];
        ret += state[j];
        i = j;
    }
    return ret;
}

floatval_t crf1d_context_t::crf1dc_lognorm()
{
    return this->log_norm;
}

floatval_t crf1d_context_t::crf1dc_viterbi(std::vector<int>& labels)
{
    int i, j, t;
    int *back = NULL;
    floatval_t max_score, score, *cur = NULL;
    int argmax_score;
    const floatval_t *prev = NULL, *state = NULL, *trans = NULL;
    const int T = this->num_items;
    const int L = this->num_labels;

    /*
        This function assumes state and trans scores to be in the logarithm domain.
     */

    /* Compute the scores at (0, *). */
    cur = ALPHA_SCORE(this, 0);
    state = STATE_SCORE(this, 0);
    for (j = 0;j < L;++j) {
        cur[j] = state[j];
    }

    /* Compute the scores at (t, *). */
    for (t = 1;t < T;++t) {
        prev = ALPHA_SCORE(this, t-1);
        cur = ALPHA_SCORE(this, t);
        state = STATE_SCORE(this, t);
        back = BACKWARD_EDGE_AT(this, t);

        /* Compute the score of (t, j). */
        for (j = 0;j < L;++j) {
            max_score = -FLOAT_MAX;
            argmax_score = -1;
            for (i = 0;i < L;++i) {
                /* Transit from (t-1, i) to (t, j). */
                trans = TRANS_SCORE(this, i);
                score = prev[i] + trans[j];

                /* Store this path if it has the maximum score. */
                if (max_score < score) {
                    max_score = score;
                    argmax_score = i;
                }
            }
            /* Backward link (#t, #j) -> (#t-1, #i). */
            if (argmax_score >= 0) back[j] = argmax_score;
            /* Add the state score on (t, j). */
            cur[j] = max_score + state[j];
        }
    }

    /* Find the node (#T, #i) that reaches EOS with the maximum score. */
    max_score = -FLOAT_MAX;
    prev = ALPHA_SCORE(this, T-1);
    /* Set a score for T-1 to be overwritten later. Just in case we don't
       end up with something beating -FLOAT_MAX. */
    labels[T-1] = 0;
    for (i = 0;i < L;++i) {
        if (max_score < prev[i]) {
            max_score = prev[i];
            labels[T-1] = i;        /* Tag the item #T. */
        }
    }

    /* Tag labels by tracing the backward links. */
    for (t = T-2;0 <= t;--t) {
        back = BACKWARD_EDGE_AT(this, t+1);
        labels[t] = back[labels[t+1]];
    }

    /* Return the maximum score (without the normalization factor subtracted). */
    return max_score;
}

static void check_values(FILE *fp, floatval_t cv, floatval_t tv)
{
    if (fabs(cv - tv) < 1e-9) {
        fprintf(fp, "OK (%f)\n", cv);
    } else {
        fprintf(fp, "FAIL: %f (%f)\n", cv, tv);
    }
}

void crf1dc_debug_context(FILE *fp)
{
    int y1, y2, y3;
    floatval_t norm = 0;
    const int L = 3;
    const int T = 3;
    crf1d_context_t *ctx = new crf1d_context_t(CTXF_MARGINALS, L, T);
    floatval_t *trans = NULL, *state = NULL;
    floatval_t scores[3][3][3];

    /* Initialize the state scores. */
    state = EXP_STATE_SCORE(ctx, 0);
    state[0] = .4;    state[1] = .5;    state[2] = .1;
    state = EXP_STATE_SCORE(ctx, 1);
    state[0] = .4;    state[1] = .1;    state[2] = .5;
    state = EXP_STATE_SCORE(ctx, 2);
    state[0] = .4;    state[1] = .1;    state[2] = .5;

    /* Initialize the transition scores. */
    trans = EXP_TRANS_SCORE(ctx, 0);
    trans[0] = .3;    trans[1] = .1;    trans[2] = .4;
    trans = EXP_TRANS_SCORE(ctx, 1);
    trans[0] = .6;    trans[1] = .2;    trans[2] = .1;
    trans = EXP_TRANS_SCORE(ctx, 2);
    trans[0] = .5;    trans[1] = .2;    trans[2] = .1;

    ctx->num_items = ctx->cap_items;
    ctx->crf1dc_alpha_score();
    ctx->crf1dc_beta_score();

    /* Compute the score of every label sequence. */
    for (y1 = 0;y1 < L;++y1) {
        floatval_t s1 = EXP_STATE_SCORE(ctx, 0)[y1];
        for (y2 = 0;y2 < L;++y2) {
            floatval_t s2 = s1;
            s2 *= EXP_TRANS_SCORE(ctx, y1)[y2];
            s2 *= EXP_STATE_SCORE(ctx, 1)[y2];
            for (y3 = 0;y3 < L;++y3) {
                floatval_t s3 = s2;
                s3 *= EXP_TRANS_SCORE(ctx, y2)[y3];
                s3 *= EXP_STATE_SCORE(ctx, 2)[y3];
                scores[y1][y2][y3] = s3;
            }
        }
    }

    /* Compute the partition factor. */
    norm = 0.;
    for (y1 = 0;y1 < L;++y1) {
        for (y2 = 0;y2 < L;++y2) {
            for (y3 = 0;y3 < L;++y3) {
                norm += scores[y1][y2][y3];
            }
        }
    }

    /* Check the partition factor. */
    fprintf(fp, "Check for the partition factor... ");
    check_values(fp, exp(ctx->log_norm), norm);

    /* Compute the sequence probabilities. */
    for (y1 = 0;y1 < L;++y1) {
        for (y2 = 0;y2 < L;++y2) {
            for (y3 = 0;y3 < L;++y3) {
                floatval_t logp;

                logp = ctx->crf1dc_score({y1, y2, y3}) - ctx->crf1dc_lognorm();

                fprintf(fp, "Check for the sequence %d-%d-%d... ", y1, y2, y3);
                check_values(fp, exp(logp), scores[y1][y2][y3] / norm);
            }
        }
    }

    /* Compute the marginal probability at t=0 */
    for (y1 = 0;y1 < L;++y1) {
        floatval_t a, b, c, s = 0.;
        for (y2 = 0;y2 < L;++y2) {
            for (y3 = 0;y3 < L;++y3) {
                s += scores[y1][y2][y3];
            }
        }

        a = ALPHA_SCORE(ctx, 0)[y1];
        b = BETA_SCORE(ctx, 0)[y1];
        c = 1. / ctx->scale_factor[0];

        fprintf(fp, "Check for the marginal probability (0,%d)... ", y1);
        check_values(fp, a * b * c, s / norm);
    }

    /* Compute the marginal probability at t=1 */
    for (y2 = 0;y2 < L;++y2) {
        floatval_t a, b, c, s = 0.;
        for (y1 = 0;y1 < L;++y1) {
            for (y3 = 0;y3 < L;++y3) {
                s += scores[y1][y2][y3];
            }
        }

        a = ALPHA_SCORE(ctx, 1)[y2];
        b = BETA_SCORE(ctx, 1)[y2];
        c = 1. / ctx->scale_factor[1];

        fprintf(fp, "Check for the marginal probability (1,%d)... ", y2);
        check_values(fp, a * b * c, s / norm);
    }

    /* Compute the marginal probability at t=2 */
    for (y3 = 0;y3 < L;++y3) {
        floatval_t a, b, c, s = 0.;
        for (y1 = 0;y1 < L;++y1) {
            for (y2 = 0;y2 < L;++y2) {
                s += scores[y1][y2][y3];
            }
        }

        a = ALPHA_SCORE(ctx, 2)[y3];
        b = BETA_SCORE(ctx, 2)[y3];
        c = 1. / ctx->scale_factor[2];

        fprintf(fp, "Check for the marginal probability (2,%d)... ", y3);
        check_values(fp, a * b * c, s / norm);
    }

    /* Compute the marginal probabilities of transitions. */
    for (y1 = 0;y1 < L;++y1) {
        for (y2 = 0;y2 < L;++y2) {
            floatval_t a, b, s, t, p = 0.;
            for (y3 = 0;y3 < L;++y3) {
                p += scores[y1][y2][y3];
            }

            a = ALPHA_SCORE(ctx, 0)[y1];
            b = BETA_SCORE(ctx, 1)[y2];
            s = EXP_STATE_SCORE(ctx, 1)[y2];
            t = EXP_TRANS_SCORE(ctx, y1)[y2];

            fprintf(fp, "Check for the marginal probability (0,%d)-(1,%d)... ", y1, y2);
            check_values(fp, a * t * s * b, p / norm);
        }
    }

    for (y2 = 0;y2 < L;++y2) {
        for (y3 = 0;y3 < L;++y3) {
            floatval_t a, b, s, t, p = 0.;
            for (y1 = 0;y1 < L;++y1) {
                p += scores[y1][y2][y3];
            }

            a = ALPHA_SCORE(ctx, 1)[y2];
            b = BETA_SCORE(ctx, 2)[y3];
            s = EXP_STATE_SCORE(ctx, 2)[y3];
            t = EXP_TRANS_SCORE(ctx, y2)[y3];

            fprintf(fp, "Check for the marginal probability (1,%d)-(2,%d)... ", y2, y3);
            check_values(fp, a * t * s * b, p / norm);
        }
    }
}
