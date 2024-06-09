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

void crf1dt_t::crf1dt_state_score(const crfsuite_instance_t *inst)
{
    crf1dt_t *crf1dt = this;
    int a, i, l, t, r, fid;
    crf1dm_feature_t f;
    feature_refs_t attr;
    floatval_t value, *state = NULL;
    crf1dm_t* model = crf1dt->model;
    crf1d_context_t* ctx = crf1dt->ctx;
    const crfsuite_item_t* item = NULL;
    const int T = inst->num_items();
    const int L = crf1dt->num_labels;

    /* Loop over the items in the sequence. */
    for (t = 0;t < T;++t) {
        item = &inst->items[t];
        state = STATE_SCORE(ctx, t);

        /* Loop over the contents (attributes) attached to the item. */
        for (i = 0;i < item->num_contents();++i) {
            /* Access the list of state features associated with the attribute. */
            a = item->contents[i].aid;
            model->crf1dm_get_attrref(a, &attr);
            /* A scale usually represents the atrribute frequency in the item. */
            value = item->contents[i].value;

            /* Loop over the state features associated with the attribute. */
            for (r = 0;r < attr.num_features;++r) {
                /* The state feature #(attr->fids[r]), which is represented by
                   the attribute #a, outputs the label #(f->dst). */
                fid = attr.crf1dm_get_featureid( r);
                model->crf1dm_get_feature(fid, &f);
                l = f.dst;
                state[l] += f.weight * value;
            }
        }
    }
}

void crf1dt_t::crf1dt_transition_score()
{
    crf1dt_t* crf1dt = this;
    int i, r, fid;
    crf1dm_feature_t f;
    feature_refs_t edge;
    floatval_t *trans = NULL;
    crf1dm_t* model = crf1dt->model;
    crf1d_context_t* ctx = crf1dt->ctx;
    const int L = crf1dt->num_labels;

    /* Compute transition scores between two labels. */
    for (i = 0;i < L;++i) {
        trans = TRANS_SCORE(ctx, i);
        model->crf1dm_get_labelref(i, &edge);
        for (r = 0;r < edge.num_features;++r) {
            /* Transition feature from #i to #(f->dst). */
            fid = edge.crf1dm_get_featureid( r);
            model->crf1dm_get_feature(fid, &f);
            trans[f.dst] = f.weight;
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

void crf1dt_t::crf1dt_delete()
{
    crf1dt_t* crf1dt = this;
    /* Note: we don't own the model object (crf1t->model). */
    if (crf1dt->ctx != NULL) {
        delete crf1dt->ctx;
        crf1dt->ctx = NULL;
    }
    free(crf1dt);
}

crf1dt_t::crf1dt_t(crf1dm_t* crf1dm)
{
    this->num_labels = crf1dm->crf1dm_get_num_labels();
    this->num_attributes = crf1dm->crf1dm_get_num_attrs();
    this->model = crf1dm;
    this->ctx = new crf1d_context_t(CTXF_VITERBI | CTXF_MARGINALS, this->num_labels, 0);
    // TODO: assert (this->ctx != NULL);
    this->ctx->crf1dc_reset(RF_TRANS);
    this->crf1dt_transition_score();
    this->ctx->crf1dc_exp_transition();
    this->level = LEVEL_NONE;
}

/*
 *    Implementation of crfsuite_tagger_t object.
 *    This object is instantiated only by a crfsuite_model_t object.
 */

int tag_crfsuite_tagger::addref()
{
    return crfsuite_interlocked_increment(&this->nref);
}

 int tag_crfsuite_tagger::release()
{
    int count = crfsuite_interlocked_decrement(&this->nref);
    if (count == 0) {
        /* This instance is being destroyed. */
        delete ((crf1dt_t*)this->internal);
    }
    return count;
}

 int tag_crfsuite_tagger::set(crfsuite_instance_t *inst)
{
    crf1dt_t* crf1dt = (crf1dt_t*)this->internal;
    crf1d_context_t* ctx = crf1dt->ctx;
    ctx->crf1dc_set_num_items(inst->num_items());
    crf1dt->ctx->crf1dc_reset(RF_STATE);
    crf1dt->crf1dt_state_score(inst);
    crf1dt->level = LEVEL_SET;
    return 0;
}

 int tag_crfsuite_tagger::length()
{
    crf1dt_t* crf1dt = (crf1dt_t*)this->internal;
    crf1d_context_t* ctx = crf1dt->ctx;
    return ctx->num_items;
}

 int tag_crfsuite_tagger::viterbi(std::vector<int>& labels, floatval_t *ptr_score)
{
    floatval_t score;
    crf1dt_t* crf1dt = (crf1dt_t*)this->internal;
    crf1d_context_t* ctx = crf1dt->ctx;

    score = ctx->crf1dc_viterbi(labels);
    if (ptr_score != NULL) {
        *ptr_score = score;
    }

    return 0;
}

int tag_crfsuite_tagger::score(std::vector<int>& path, floatval_t *ptr_score)
{
    floatval_t score;
    crf1dt_t* crf1dt = (crf1dt_t*)this->internal;
    crf1d_context_t* ctx = crf1dt->ctx;
    score = ctx->crf1dc_score(path);
    if (ptr_score != NULL) {
        *ptr_score = score;
    }
    return 0;
}

 int tag_crfsuite_tagger::lognorm( floatval_t *ptr_norm)
{
    crf1dt_t* crf1dt = (crf1dt_t*)this->internal;
    crf1dt->crf1dt_set_level(LEVEL_ALPHABETA);
    *ptr_norm = crf1dt->ctx->crf1dc_lognorm();
    return 0;
}

 int tag_crfsuite_tagger::marginal_point( int l, int t, floatval_t *ptr_prob)
{
    crf1dt_t* crf1dt = (crf1dt_t*)this->internal;
    crf1dt->crf1dt_set_level(LEVEL_ALPHABETA);
    *ptr_prob = crf1dt->ctx->crf1dc_marginal_point(l, t);
    return 0;
}

 int tag_crfsuite_tagger::marginal_path( const int *path, int begin, int end, floatval_t *ptr_prob)
{
    crf1dt_t* crf1dt = (crf1dt_t*)this->internal;
    crf1dt->crf1dt_set_level(LEVEL_ALPHABETA);
    *ptr_prob = crf1dt->ctx->crf1dc_marginal_path(path, begin, end);
    return 0;
}



/*
 *    Implementation of crfsuite_dictionary_t object for attributes.
 *    This object is instantiated only by a crfsuite_model_t object.
 */

struct ModelAttrDict: crfsuite_dictionary_t {
private:
    crf1dm_t * crf1dm;
public:
    ModelAttrDict(crf1dm_t* crf1dm) : crf1dm(crf1dm) {}

     int get(const char *str)
    {
        /* This object is ready only. */
        throw std::runtime_error("supported");
    }

     int to_id(const char *str) { return crf1dm->crf1dm_to_aid(str); }

     int to_string(int id, char const **pstr)
    {
        *pstr = crf1dm->crf1dm_to_attr(id);
        return 0;
    }

     int num() { return crf1dm->crf1dm_get_num_attrs(); }

     void free(const char *str)
    {
        /* all strings are freed on the release of the dictionary object. */
    }

};


/*
 *    Implementation of crfsuite_dictionary_t object for labels.
 *    This object is instantiated only by a crfsuite_model_t object.
 */
struct ModelLabelsDict : crfsuite_dictionary_t
{
private:
    crf1dm_t * crf1dm;
public:
    ModelLabelsDict(crf1dm_t* crf1dm) : crf1dm(crf1dm) {}

     int addref()
    {
        /* This object is owned only by a crfsuite_model_t object. */
        return 1;
    }

     int release()
    {
        /* This object is owned and freed only by a crfsuite_model_t object. */
        return 1;
    }

     int get(const char *str)
    {
        /* This object is ready only. */
        return CRFSUITEERR_NOTSUPPORTED;
    }

     int to_id(const char *str)
    {
        return crf1dm->crf1dm_to_lid(str);
    }

     int to_string(int id, char const **pstr)
    {
        *pstr = crf1dm->crf1dm_to_label(id);
        return 0;
    }

     int num()
    {
        return crf1dm->crf1dm_get_num_labels();
    }

     void free( const char *str)
    {
        /* all strings are freed on the release of the dictionary object. */
    }

};

/*
 *    Implementation of crfsuite_model_t object.
 *    This object is instantiated by crf1m_model_create() function.
 */

 struct model_internal_t : tag_crfsuite_model{
 private:
    crf1dm_t*    crf1dm;

public:
    crfsuite_dictionary_t*    attrs;
    crfsuite_dictionary_t*    labels;
    model_internal_t(crf1dm_t*    crf1dm) : crf1dm(crf1dm) {}
     int addref()
    {
         return 1;
    }
     int release()
    {
        return 1;
    }
     int get_tagger(crfsuite_tagger_t** ptr_tagger)
    {
        int ret = 0;
        crf1dt_t *crf1dt = NULL;
        crfsuite_tagger_t *tagger = NULL;

        /* Construct a tagger based on the model. */
        crf1dt = new crf1dt_t(crf1dm);
        if (crf1dt == NULL) {
            ret = CRFSUITEERR_OUTOFMEMORY;
            goto error_exit;
        }

        /* Create an instance of tagger object. */
        tagger = (crfsuite_tagger_t*)calloc(1, sizeof(crfsuite_tagger_t));
        if (tagger == NULL) {
            ret = CRFSUITEERR_OUTOFMEMORY;
            goto error_exit;
        }
        tagger->internal = crf1dt;
        tagger->nref = 1;
        *ptr_tagger = tagger;
        return 0;

    error_exit:
        free(tagger);
        if (crf1dt != NULL) {
            delete crf1dt;
        }
        return ret;
    }
     int get_labels(crfsuite_dictionary_t** ptr_labels)
    {
        /* We don't increment the reference counter. */
        *ptr_labels = labels;
        return 0;
    }

     int get_attrs(crfsuite_dictionary_t** ptr_attrs)
    {
        /* We don't increment the reference counter. */
        *ptr_attrs = attrs;
        return 0;
    }

     int dump(FILE *fpo)
    {
        crf1dm->crf1dm_dump(fpo);
        return 0;
    }
} ;


static int crf1m_model_create(crf1dm_t *crf1dm, void** ptr_model)
{
    int ret = 0;
    crfsuite_model_t *model = NULL;
    crfsuite_dictionary_t *attrs = NULL, *labels = NULL;

    *ptr_model = NULL;

    if (crf1dm == NULL) {
        throw std::runtime_error("unsupported");
    }

    /* Create an instance of internal data attached to the model. */
    model_internal_t *internal = new model_internal_t(crf1dm);

    /* Create an instance of dictionary object for attributes. */
    attrs = new ModelAttrDict(crf1dm);

    /* Create an instance of dictionary object for labels. */
    labels = new ModelLabelsDict(crf1dm);

    /* Set the internal data for the model object. */
    internal->attrs = attrs;
    internal->labels = labels;

    *ptr_model = model;
    return 0;
}

int crf1m_create_instance_from_file(const char *filename, void **ptr)
{
    return crf1m_model_create(new tag_crf1dm(filename), ptr);
}

int crf1m_create_instance_from_memory(const void *data, size_t size, void **ptr)
{
    return crf1m_model_create(new tag_crf1dm(data, size), ptr);
}
