/*
 *      CRF1d tagger (implementation of crfsuite_model_t and crfsuite_tagger_t).
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <crfsuite.h>

#include "crf1d.h"

enum {
    LEVEL_NONE = 0,
    LEVEL_SET,
    LEVEL_ALPHABETA,
};

void crf1dt_t::crf1dt_state_score(const crfsuite_instance_t &inst)
{
    crf1dm_feature_t f;
    feature_refs_t attr;
    const int T = inst.num_items();
    const int L = this->model->crf1dm_get_num_labels();

    /* Loop over the items in the sequence. */
    for (int t = 0;t < T;++t) {
        const crfsuite_item_t& item = inst.items[t];
        floatval_t* state = STATE_SCORE(this->ctx, t);

        /* Loop over the contents (attributes) attached to the item. */
        for (int i = 0;i < item.num_contents();++i) {
            /* Access the list of state features associated with the attribute. */
            int a = item.contents[i].aid;
            this->model->crf1dm_get_attrref(a, &attr);
            /* A scale usually represents the atrribute frequency in the item. */
            floatval_t value = item.contents[i].value;

            /* Loop over the state features associated with the attribute. */
            for (int r = 0;r < attr.num_features;++r) {
                /* The state feature #(attr->fids[r]), which is represented by
                   the attribute #a, outputs the label #(f->dst). */
                int fid = this->model->crf1dm_get_featureid(&attr, r);
                this->model->crf1dm_get_feature(fid, &f);
                int l = f.dst;
                state[l] += f.weight * value;
            }
        }
    }
}

void crf1dt_t::crf1dt_set_level(int level)
{
    crf1dt_t *crf1dt = this;
    int prev = crf1dt->level;
    crf1d_context_t* ctx = crf1dt->ctx;

    if (level <= LEVEL_ALPHABETA && prev < LEVEL_ALPHABETA) {
        ctx->crf1dc_exp_state();
        ctx->crf1dc_alpha_score();
        ctx->crf1dc_beta_score();
    }

    crf1dt->level = level;
}

crf1dt_t::crf1dt_t(crf1dm_t* crf1dm)
{
    auto L = crf1dm->crf1dm_get_num_labels();
    auto A = crf1dm->crf1dm_get_num_attrs();
    this->model = crf1dm;
    this->ctx = new crf1d_context_t(CTXF_VITERBI | CTXF_MARGINALS, L, 0);
    this->ctx->crf1dc_reset(RF_TRANS);
    {
        crf1dm_feature_t f;
        feature_refs_t edge;

        /* Compute transition scores between two labels. */
        for (int i = 0;i < L;++i) {
            floatval_t *trans = &this->ctx->trans[this->ctx->num_labels * i];
            this->model->crf1dm_get_labelref(i, &edge);
            for (int r = 0; r < edge.num_features; ++r) {
                /* Transition feature from #i to #(f->dst). */
                int fid = this->model->crf1dm_get_featureid(&edge, r);
                this->model->crf1dm_get_feature(fid, &f);
                trans[f.dst] = f.weight;
            }
        }
    }
    this->ctx->crf1dc_exp_transition();
    this->level = LEVEL_NONE;
}

int crf1dt_t::set(const crfsuite_instance_t &inst)
{
    this->ctx->crf1dc_set_num_items(inst.num_items());
    this->ctx->crf1dc_reset(RF_STATE);
    this->crf1dt_state_score(inst);
    this->level = LEVEL_SET;
    return 0;
}

floatval_t crf1dt_t::lognorm()
{
    this->crf1dt_set_level(LEVEL_ALPHABETA);
    return this->ctx->crf1dc_lognorm();
}

floatval_t crf1dt_t::marginal_point( int l, int t)
{
    this->crf1dt_set_level(LEVEL_ALPHABETA);
    return this->ctx->crf1dc_marginal_point(l, t);
}

floatval_t crf1dt_t::marginal_path( const int *path, int begin, int end)
{
    this->crf1dt_set_level(LEVEL_ALPHABETA);
    return this->ctx->crf1dc_marginal_path(path, begin, end);
}

int crf1m_create_instance_from_file(const char *filename, void **ptr)
{
    *ptr = new tag_crf1dm(filename);
    return 0;
}

int crf1m_create_instance_from_memory(const void *data, size_t size, void **ptr)
{
    *ptr = new tag_crf1dm(data, size);
    return 0;
}
