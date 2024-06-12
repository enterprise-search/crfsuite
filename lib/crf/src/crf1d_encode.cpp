/*
 *      CRF1d encoder (routines for training).
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

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>

#include <crfsuite.h>
#include "crfsuite_internal.h"
#include "crf1d.h"
#include "params.h"
#include "logging.h"

/**
 * Parameters for feature generation.
 */
 struct crf1de_option_t{
    floatval_t  feature_minfreq;                /** The threshold for occurrences of features. */
    int         feature_possible_states;        /** Dense state features. */
    int         feature_possible_transitions;   /** Dense transition features. */
} ;
#define    FEATURE(crf1de, k) \
    (&(crf1de)->features[(k)])
#define    ATTRIBUTE(crf1de, a) \
    (&(crf1de)->attributes[(a)])
#define    TRANSITION(crf1de, i) \
    (&(crf1de)->forward_trans[(i)])
/**
 * CRF1d internal data.
 */
 struct crf1de_t{
    std::vector<crf1df_feature_t> features;     /**< Array of feature descriptors [K]. */
    std::vector<feature_refs_t> attributes;     /**< References to attribute features [A]. */
    std::vector<feature_refs_t> forward_trans;  /**< References to transition features [L]. */

    crf1d_context_t *ctx;           /**< CRF1d context. */
    crf1de_option_t opt;            /**< CRF1d options. */
public:
    crf1de_t() {}
    size_t num_labels() const { return this->forward_trans.size(); }
    void state_score(const crfsuite_instance_t& inst,const floatval_t* w)
    {
        int i, t, r;
        crf1d_context_t* ctx = this->ctx;
        const int T = inst.num_items();
        const int L = this->num_labels();

        /* Loop over the items in the sequence. */
        for (t = 0;t < T;++t) {
            const crfsuite_item_t *item = &inst.items[t];
            floatval_t *state = STATE_SCORE(ctx, t);

            /* Loop over the contents (attributes) attached to the item. */
            for (const auto & content: item->contents) {
                /* Access the list of state features associated with the attribute. */
                int a = content.aid;
                const feature_refs_t& attr = this->attributes[a];
                floatval_t value = content.value;

                /* Loop over the state features associated with the attribute. */
                for (r = 0;r < attr.num_features;++r) {
                    /* State feature associates the attribute #a with the label #(f->dst). */
                    int fid = attr.fids[r];
                    const crf1df_feature_t f = this->features[fid];
                    state[f.dst] += w[fid] * value;
                }
            }
        }
    }
     void
    state_score_scaled(const crfsuite_instance_t* inst,const floatval_t* w,const floatval_t scale)
    {
        int i, t, r;
        crf1d_context_t* ctx = this->ctx;
        const int T = inst->num_items();
        const int L = this->num_labels();

        /* Forward to the non-scaling version for fast computation when scale == 1. */
        if (scale == 1.) {
            this->state_score( *inst, w);
            return;
        }

        /* Loop over the items in the sequence. */       
        for (t = 0;t < T;++t) {
            const crfsuite_item_t *item = &inst->items[t];
            floatval_t *state = STATE_SCORE(ctx, t);

            /* Loop over the contents (attributes) attached to the item. */
            for (i = 0;i < item->num_contents();++i) {
                /* Access the list of state features associated with the attribute. */
                int a = item->contents[i].aid;
                const feature_refs_t *attr = ATTRIBUTE(this, a);
                floatval_t value = item->contents[i].value * scale;

                /* Loop over the state features associated with the attribute. */
                for (r = 0;r < attr->num_features;++r) {
                    /* State feature associates the attribute #a with the label #(f->dst). */
                    int fid = attr->fids[r];
                    const crf1df_feature_t *f = FEATURE(this, fid);
                    state[f->dst] += w[fid] * value;
                }
            }
        }
    }
    void transition_score(const floatval_t* w)
    {
        int i, r;
        crf1d_context_t* ctx = this->ctx;
        const int L = this->num_labels();

        /* Compute transition scores between two labels. */
        for (i = 0;i < L;++i) {
            floatval_t *trans = TRANS_SCORE(ctx, i);
            const feature_refs_t *edge = TRANSITION(this, i);
            for (r = 0;r < edge->num_features;++r) {
                /* Transition feature from #i to #(f->dst). */
                int fid = edge->fids[r];
                const crf1df_feature_t *f = FEATURE(this, fid);
                trans[f->dst] = w[fid];
            }        
        }
    }
    void transition_score_scaled(const floatval_t* w, const floatval_t scale)
    {
        int i, r;
        crf1d_context_t* ctx = this->ctx;
        const int L = this->num_labels();

        /* Forward to the non-scaling version for fast computation when scale == 1. */
        if (scale == 1.) {
            this->transition_score( w);
            return;
        }

        /* Compute transition scores between two labels. */
        for (i = 0;i < L;++i) {
            floatval_t *trans = TRANS_SCORE(ctx, i);
            const feature_refs_t *edge = TRANSITION(this, i);
            for (r = 0;r < edge->num_features;++r) {
                /* Transition feature from #i to #(f->dst). */
                int fid = edge->fids[r];
                const crf1df_feature_t *f = FEATURE(this, fid);
                trans[f->dst] = w[fid] * scale;
            }        
        }
    }


    void
    features_on_path(
        const crfsuite_instance_t *inst,
        const std::vector<int>& labels,
        crfsuite_encoder_features_on_path_callback func,
        void *instance
        )
    {
        int c, i = -1, t, r;
        crf1d_context_t* ctx = this->ctx;
        const int T = inst->num_items();
        const int L = this->num_labels();

        /* Loop over the items in the sequence. */
        for (t = 0;t < T;++t) {
            const crfsuite_item_t *item = &inst->items[t];
            const int j = labels[t];

            /* Loop over the contents (attributes) attached to the item. */
            for (c = 0;c < item->num_contents();++c) {
                /* Access the list of state features associated with the attribute. */
                int a = item->contents[c].aid;
                const feature_refs_t *attr = ATTRIBUTE(this, a);
                floatval_t value = item->contents[c].value;

                /* Loop over the state features associated with the attribute. */
                for (r = 0;r < attr->num_features;++r) {
                    /* State feature associates the attribute #a with the label #(f->dst). */
                    int fid = attr->fids[r];
                    const crf1df_feature_t *f = FEATURE(this, fid);
                    if (f->dst == j) {
                        func(instance, fid, value);
                    }
                }
            }

            if (i != -1) {
                const feature_refs_t *edge = TRANSITION(this, i);
                for (r = 0;r < edge->num_features;++r) {
                    /* Transition feature from #i to #(f->dst). */
                    int fid = edge->fids[r];
                    const crf1df_feature_t *f = FEATURE(this, fid);
                    if (f->dst == j) {
                        func(instance, fid, 1.);
                    }
                }
            }

            i = j;
        }
    }

    void
    observation_expectation(
        const crfsuite_instance_t* inst,
        const std::vector<int>& labels,
        floatval_t *w,
        const floatval_t scale
        )
    {
        int c, i = -1, t, r;
        crf1d_context_t* ctx = this->ctx;
        const int T = inst->num_items();
        const int L = this->num_labels();

        /* Loop over the items in the sequence. */
        for (t = 0;t < T;++t) {
            const crfsuite_item_t *item = &inst->items[t];
            const int j = labels[t];

            /* Loop over the contents (attributes) attached to the item. */
            for (c = 0;c < item->num_contents();++c) {
                /* Access the list of state features associated with the attribute. */
                int a = item->contents[c].aid;
                const feature_refs_t *attr = ATTRIBUTE(this, a);
                floatval_t value = item->contents[c].value;

                /* Loop over the state features associated with the attribute. */
                for (r = 0;r < attr->num_features;++r) {
                    /* State feature associates the attribute #a with the label #(f->dst). */
                    int fid = attr->fids[r];
                    const crf1df_feature_t *f = FEATURE(this, fid);
                    if (f->dst == j) {
                        w[fid] += value * scale;
                    }
                }
            }

            if (i != -1) {
                const feature_refs_t *edge = TRANSITION(this, i);
                for (r = 0;r < edge->num_features;++r) {
                    /* Transition feature from #i to #(f->dst). */
                    int fid = edge->fids[r];
                    const crf1df_feature_t *f = FEATURE(this, fid);
                    if (f->dst == j) {
                        w[fid] += scale;
                    }
                }
            }

            i = j;
        }
    }
    void
    model_expectation(
        const crfsuite_instance_t *inst,
        floatval_t *w,
        const floatval_t scale
        )
    {
        int a, c, i, t, r;
        crf1d_context_t* ctx = this->ctx;
        const feature_refs_t *attr = NULL, *trans = NULL;
        const crfsuite_item_t* item = NULL;
        const int T = inst->num_items();
        const int L = this->num_labels();

        for (t = 0;t < T;++t) {
            floatval_t *prob = STATE_MEXP(ctx, t);

            /* Compute expectations for state features at position #t. */
            item = &inst->items[t];
            for (c = 0;c < item->num_contents();++c) {
                /* Access the attribute. */
                floatval_t value = item->contents[c].value;
                a = item->contents[c].aid;
                attr = ATTRIBUTE(this, a);

                /* Loop over state features for the attribute. */
                for (r = 0;r < attr->num_features;++r) {
                    int fid = attr->fids[r];
                    crf1df_feature_t *f = FEATURE(this, fid);
                    w[fid] += prob[f->dst] * value * scale;
                }
            }
        }

        /* Loop over the labels (t, i) */
        for (i = 0;i < L;++i) {
            const floatval_t *prob = TRANS_MEXP(ctx, i);
            const feature_refs_t *edge = TRANSITION(this, i);
            for (r = 0;r < edge->num_features;++r) {
                /* Transition feature from #i to #(f->dst). */
                int fid = edge->fids[r];
                crf1df_feature_t *f = FEATURE(this, fid);
                w[fid] += prob[f->dst] * scale;
            }
        }
    }

    void set_data(dataset_t &ds,logging_t *lg)
    {
        clock_t begin = 0;
        int T = 0;
        const int L = ds.data->labels->num();
        const int A = ds.data->attrs->num();
        const int N = ds.num_instances;
        crf1de_option_t *opt = &this->opt;

        /* Find the maximum length of items in the data set. */
        for (int i = 0;i < N;++i) {
            const crfsuite_instance_t *inst = ds.get( i);
            if (T < inst->num_items()) {
                T = inst->num_items();
            }
        }

        /* Construct a CRF context. */
        this->ctx = new crf1d_context_t(CTXF_MARGINALS | CTXF_VITERBI, L, T);
        if (this->ctx == NULL) {
            throw std::runtime_error("OOM");
        }

        /* Feature generation. */
        logging(lg, "Feature generation\n");
        logging(lg, "type: CRF1d\n");
        logging(lg, "feature.minfreq: %f\n", opt->feature_minfreq);
        logging(lg, "feature.possible_states: %d\n", opt->feature_possible_states);
        logging(lg, "feature.possible_transitions: %d\n", opt->feature_possible_transitions);
        begin = clock();
        crf1df_generate(
            this->features,
            ds,
            opt->feature_possible_states ? 1 : 0,
            opt->feature_possible_transitions ? 1 : 0,
            opt->feature_minfreq,
            lg->func,
            lg->instance
            );
        auto num_features = this->features.size();
        logging(lg, "Number of features: %d\n", num_features);
        logging(lg, "Seconds required: %.3f\n", (clock() - begin) / (double)CLOCKS_PER_SEC);
        logging(lg, "\n");

        /* Initialize the feature references. */
        this->attributes = std::vector<feature_refs_t>(A);
        this->forward_trans = std::vector<feature_refs_t>(L);
        crf1df_init_references(
            this->attributes,
            this->forward_trans,
            this->features)
           ;
    }


    int save_model(const char *filename,const std::vector<floatval_t>& w, crfsuite_dictionary_t *attrs, crfsuite_dictionary_t *labels, logging_t *lg)
    {
    clock_t begin;
    
    const floatval_t threshold = 0.01;
    const int L = this->num_labels();
    const int A = this->attributes.size();
    const int K = this->features.size();
    int J = 0, B = 0;

    /* Start storing the model. */
    logging(lg, "Storing the model\n");
    begin = clock();

    /* Allocate and initialize the feature mapping. */
    int *fmap = new int[K];
    
#ifdef  CRF_TRAIN_SAVE_NO_PRUNING
    for (int k = 0;k < K;++k) fmap[k] = k;
    J = K;
#else
    for (int k = 0;k < K;++k) fmap[k] = -1;
#endif/*CRF_TRAIN_SAVE_NO_PRUNING*/

    /* Allocate and initialize the attribute mapping. */
    int *amap = new int[A];    
#ifdef  CRF_TRAIN_SAVE_NO_PRUNING
    for (a = 0;a < A;++a) amap[a] = a;
    B = A;
#else
    for (int a = 0;a < A;++a) amap[a] = -1;
#endif/*CRF_TRAIN_SAVE_NO_PRUNING*/

    /*
     *  Open a model writer.
     */
    tag_crf1dmw* writer = new tag_crf1dmw(filename);

    /* Open a feature chunk in the model file. */
    writer->crf1dmw_open_features(K);

    /*
     *  Write the feature values.
     *     (with determining active features and attributes).
     */
    for (int k = 0;k < K;++k) {
        crf1df_feature_t* f = &this->features[k];
        if (w[k] != 0) {
            int src;
            crf1dm_feature_t feat;

#ifndef CRF_TRAIN_SAVE_NO_PRUNING
            /* The feature (#k) will have a new feature id (#J). */
            fmap[k] = J++;        /* Feature #k -> #fmap[k]. */

            /* Map the source of the field. */
            if (f->type == FT_STATE) {
                /* The attribute #(f->src) will have a new attribute id (#B). */
                if (amap[f->src] < 0) amap[f->src] = B++;    /* Attribute #a -> #amap[a]. */
                src = amap[f->src];
            } else {
                src = f->src;
            }
#endif/*CRF_TRAIN_SAVE_NO_PRUNING*/

            feat.type = f->type;
            feat.src = src;
            feat.dst = f->dst;
            feat.weight = w[k];

            /* Write the feature. */
            writer->crf1dmw_put_feature(fmap[k], &feat);
        }
    }

    /* Close the feature chunk. */
    writer->crf1dmw_close_features();

    logging(lg, "Number of active features: %d (%d)\n", J, K);
    logging(lg, "Number of active attributes: %d (%d)\n", B, A);
    logging(lg, "Number of active labels: %d (%d)\n", L, L);

    /* Write labels. */
    logging(lg, "Writing labels\n", L);
    writer->crf1dmw_open_labels(L);
    for (int l = 0;l < L;++l) {
        const char *str = NULL;
        labels->to_string( l, &str);
        if (str != NULL) {
            writer->crf1dmw_put_label(l, str);
            labels->free( str);
        }
    }
    writer->crf1dmw_close_labels();

    /* Write attributes. */
    logging(lg, "Writing attributes\n");
    writer->crf1dmw_open_attrs(B);
    for (int a = 0;a < A;++a) {
        if (0 <= amap[a]) {
            const char *str = NULL;
            attrs->to_string( a, &str);
            if (str != NULL) {
                writer->crf1dmw_put_attr(amap[a], str);
                attrs->free( str);
            }
        }
    }
    writer->crf1dmw_close_attrs();

    /* Write label feature references. */
    logging(lg, "Writing feature references for transitions\n");

    writer->crf1dmw_open_labelrefs(L+2);
    for (int l = 0;l < L;++l) {
        const feature_refs_t *edge = TRANSITION(this, l);
        writer->crf1dmw_put_labelref(l, edge, fmap);
    }
    writer->crf1dmw_close_labelrefs();

    /* Write attribute feature references. */
    logging(lg, "Writing feature references for attributes\n");
     writer->crf1dmw_open_attrrefs(B);
    for (int a = 0;a < A;++a) {
        if (0 <= amap[a]) {
            const feature_refs_t *attr = ATTRIBUTE(this, a);
            writer->crf1dmw_put_attrref(amap[a], attr, fmap);
        }
    }
    writer->crf1dmw_close_attrrefs();

    /* Close the writer. */
    delete writer;
    logging(lg, "Seconds required: %.3f\n", (clock() - begin) / (double)CLOCKS_PER_SEC);
    logging(lg, "\n");

    delete[] amap;
    delete[] fmap;
    return 0;
    }
};

static int crf1de_exchange_options(crfsuite_params_t* params, crf1de_option_t* opt, int mode)
{
    BEGIN_PARAM_MAP(params, mode)
        DDX_PARAM_FLOAT(
            "feature.minfreq", opt->feature_minfreq, 0.0,
            "The minimum frequency of features."
            )
        DDX_PARAM_INT(
            "feature.possible_states", opt->feature_possible_states, 0,
            "Force to generate possible state features."
            )
        DDX_PARAM_INT(
            "feature.possible_transitions", opt->feature_possible_transitions, 0,
            "Force to generate possible transition features."
            )
    END_PARAM_MAP()

    return 0;
}

/*
 *    Implementation of encoder_t object.
 */

enum {
    /** No precomputation. */
    LEVEL_NONE = 0,
    /** Feature weights are set. */
    LEVEL_WEIGHT,
    /** Instance is set. */
    LEVEL_INSTANCE,
    /** Performed the forward-backward algorithm. */
    LEVEL_ALPHABETA,
    /** Computed marginal probabilities. */
    LEVEL_MARGINAL,
};

void tag_encoder::set_level(int level)
{
    int prev = this->level;
    crf1de_t *crf1de = (crf1de_t*)this->internal;

    /*
        Each training algorithm has a different requirement for processing a
        training instance. For example, the perceptron algorithm need compute
        Viterbi paths whereas gradient-based algorithms (e.g., SGD) need
        marginal probabilities computed by the forward-backward algorithm.
     */

    /* LEVEL_WEIGHT: set transition scores. */
    if (LEVEL_WEIGHT <= level && prev < LEVEL_WEIGHT) {
        crf1de->ctx->crf1dc_reset(RF_TRANS);
        crf1de->transition_score_scaled(this->w, this->scale);
    }

    /* LEVEL_INSTANCE: set state scores. */
    if (LEVEL_INSTANCE <= level && prev < LEVEL_INSTANCE) {
        crf1de->ctx->crf1dc_set_num_items(this->inst->num_items());
        crf1de->ctx->crf1dc_reset(RF_STATE);
        crf1de->state_score_scaled(this->inst, this->w, this->scale);
    }

    /* LEVEL_ALPHABETA: perform the forward-backward algorithm. */
    if (LEVEL_ALPHABETA <= level && prev < LEVEL_ALPHABETA) {
        crf1de->ctx->crf1dc_exp_transition();
        crf1de->ctx->crf1dc_exp_state();
        crf1de->ctx->crf1dc_alpha_score();
        crf1de->ctx->crf1dc_beta_score();
    }

    /* LEVEL_MARGINAL: compute the marginal probability. */
    if (LEVEL_MARGINAL <= level && prev < LEVEL_MARGINAL) {
        crf1de->ctx->crf1dc_marginals();
    }

    this->level = level;
}

int tag_encoder::exchange_options(crfsuite_params_t* params, int mode)
{
    crf1de_t *crf1de = (crf1de_t*)this->internal;
    return crf1de_exchange_options(params, &crf1de->opt, mode);
}

void tag_encoder::set_data(dataset_t &ds, logging_t *lg)
{
    crf1de_t *crf1de = (crf1de_t*)this->internal;

    crf1de->set_data(ds,lg);
    this->num_features = crf1de->features.size();
    this->cap_items = crf1de->ctx->cap_items;
}

/* LEVEL_NONE -> LEVEL_NONE. */
void tag_encoder::objective_and_gradients_batch(dataset_t& ds, const floatval_t *w, floatval_t *f, floatval_t *g)
{
    floatval_t logp = 0, logl = 0;
    crf1de_t *crf1de = (crf1de_t*)this->internal;
    const int N = ds.num_instances;
    const int K = crf1de->features.size();

    /*
        Initialize the gradients with observation expectations.
     */
    for (int i = 0;i < K;++i) {
        crf1df_feature_t* f = &crf1de->features[i];
        g[i] = -f->freq;
    }

    /*
        Set the scores (weights) of transition features here because
        these are independent of input label sequences.
     */
    crf1de->ctx->crf1dc_reset(RF_TRANS);
    crf1de->transition_score( w);
    crf1de->ctx->crf1dc_exp_transition();

    /*
        Compute model expectations.
     */
    for (int i = 0;i < N;++i) {
        const crfsuite_instance_t *seq = ds.get( i);

        /* Set label sequences and state scores. */
        crf1de->ctx->crf1dc_set_num_items(seq->num_items());
        crf1de->ctx->crf1dc_reset(RF_STATE);
        crf1de->state_score(*seq, w);
        crf1de->ctx->crf1dc_exp_state();

        /* Compute forward/backward scores. */
        crf1de->ctx->crf1dc_alpha_score();
        crf1de->ctx->crf1dc_beta_score();
        crf1de->ctx->crf1dc_marginals();

        /* Compute the probability of the input sequence on the model. */
        logp = crf1de->ctx->crf1dc_score(seq->labels) - crf1de->ctx->crf1dc_lognorm();
        /* Update the log-likelihood. */
        logl += logp * seq->weight;

        /* Update the model expectations of features. */
        crf1de->model_expectation( seq, g, seq->weight);
    }

    *f = -logl;
}

/* LEVEL_NONE -> LEVEL_NONE. */
void tag_encoder::features_on_path(const crfsuite_instance_t *inst, const std::vector<int>& path, crfsuite_encoder_features_on_path_callback func, void *instance)
{
    crf1de_t *crf1de = (crf1de_t*)this->internal;
    crf1de->features_on_path( inst, path, func, instance);
}

/* LEVEL_NONE -> LEVEL_NONE. */
void tag_encoder::save_model(const char *filename, const dataset_t& ds, const std::vector<floatval_t> &w, logging_t *lg)
{
    crf1de_t *crf1de = (crf1de_t*)this->internal;
    crf1de->save_model( filename, w, ds.data->attrs,  ds.data->labels, lg);
}

/* LEVEL_NONE -> LEVEL_WEIGHT. */
void tag_encoder::set_weights(const floatval_t *w, floatval_t scale)
{
    this->w = w;
    this->scale = scale;
    this->level = LEVEL_WEIGHT-1;
    this->set_level(LEVEL_WEIGHT);
}

/* LEVEL_WEIGHT -> LEVEL_INSTANCE. */
void tag_encoder::set_instance(const crfsuite_instance_t *inst)
{
    this->inst = inst;
    this->level = LEVEL_INSTANCE-1;
    this->set_level(LEVEL_INSTANCE);
}

/* LEVEL_INSTANCE -> LEVEL_INSTANCE. */
floatval_t tag_encoder::score(const std::vector<int>& path)
{
    crf1de_t *crf1de = (crf1de_t*)this->internal;
    return crf1de->ctx->crf1dc_score(path);
}

/* LEVEL_INSTANCE -> LEVEL_INSTANCE. */
floatval_t tag_encoder::viterbi(std::vector<int>& path)
{
    crf1de_t *crf1de = (crf1de_t*)this->internal;
    return crf1de->ctx->crf1dc_viterbi(path);
}

/* LEVEL_INSTANCE -> LEVEL_ALPHABETA. */
floatval_t tag_encoder::partition_factor()
{
    crf1de_t *crf1de = (crf1de_t*)this->internal;
    this->set_level(LEVEL_ALPHABETA);
    return crf1de->ctx->crf1dc_lognorm();
}

/* LEVEL_INSTANCE -> LEVEL_MARGINAL. */
floatval_t tag_encoder::objective_and_gradients( floatval_t *g, floatval_t gain, floatval_t weight)
{
    crf1de_t *crf1de = (crf1de_t*)this->internal;
    this->set_level(LEVEL_MARGINAL);
    gain *= weight;
    crf1de->observation_expectation( this->inst, this->inst->labels, g, gain);
    crf1de->model_expectation( this->inst, g, -gain);
    return (-crf1de->ctx->crf1dc_score(this->inst->labels) + crf1de->ctx->crf1dc_lognorm()) * weight;
}

tag_encoder::~tag_encoder()
{
    crf1de_t *crf1de = (crf1de_t*)this->internal;
    delete (crf1de);
}

tag_encoder::tag_encoder()
{
    this->internal = new crf1de_t();
}
