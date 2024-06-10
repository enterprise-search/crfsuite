/*
 *      CRF1d feature generator (dyad features).
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
#include <string.h>
#include <unordered_set>

#include <crfsuite.h>

#include "logging.h"
#include "crf1d.h"
// #include "rumavl.h"    /* AVL tree library necessary for feature generation. */

/**
 * Feature set.
 */

struct FeatureEqual {
    bool operator()(const crf1df_feature_t& lhs, const crf1df_feature_t& rhs) const {
        return lhs.type == rhs.type && lhs.src == rhs.src && lhs.dst == rhs.dst;
    }
};

struct FeatureHash {
    int operator()(const crf1df_feature_t& f) {
        return f.type + f.src + f.dst;
    }
};

struct featureset_t
{
std::unordered_set<crf1df_feature_t, FeatureHash, FeatureEqual> m;
public:
    void add(const crf1df_feature_t& f)
    {
        auto p = this->m.find(f);
        if (p != this->m.end()) {
            crf1df_feature_t o = *p;
            o.freq += f.freq;
            this->m.erase(p);
            this->m.insert(o);
        } else {
            this->m.insert(f);
        }
    }
    void featureset_generate(std::vector<crf1df_feature_t>& features, floatval_t minfreq) const
    {
        std::copy_if(this->m.begin(), this->m.end(), std::back_inserter(features), [=](const auto& x){ return x.freq >= minfreq; });
    }
};





void crf1df_generate(
    std::vector<crf1df_feature_t>& features,
    dataset_t &ds,
    int num_labels,
    int num_attributes,
    int connect_all_attrs,
    int connect_all_edges,
    floatval_t minfreq,
    crfsuite_logging_callback func,
    void *instance
    )
{
    int c, i, j, s, t;
    crf1df_feature_t f;
    const int N = ds.num_instances;
    const int L = num_labels;
    logging_t lg;

    lg.func = func;
    lg.instance = instance;
    lg.percent = 0;

    /* Create an instance of feature set. */
    featureset_t set;

    /* Loop over the sequences in the training data. */
    logging_progress_start(&lg);

    for (s = 0;s < N;++s) {
        int prev = L, cur = 0;
        const crfsuite_item_t* item = NULL;
        const crfsuite_instance_t* seq = ds.get( s);
        const int T = seq->num_items();

        /* Loop over the items in the sequence. */
        for (t = 0;t < T;++t) {
            item = &seq->items[t];
            cur = seq->labels[t];

            /* Transition feature: label #prev -> label #(item->yid).
               Features with previous label #L are transition BOS. */
            if (prev != L) {
                f.type = FT_TRANS;
                f.src = prev;
                f.dst = cur;
                f.freq = seq->weight;
                set.add(f);
            }

            for (c = 0;c < item->num_contents();++c) {
                /* State feature: attribute #a -> state #(item->yid). */
                f.type = FT_STATE;
                f.src = item->contents[c].aid;
                f.dst = cur;
                f.freq = seq->weight * item->contents[c].value;
                set.add(f);

                /* Generate state features connecting attributes with all
                   output labels. These features are not unobserved in the
                   training data (zero expexcations). */
                if (connect_all_attrs) {
                    for (i = 0;i < L;++i) {
                        f.type = FT_STATE;
                        f.src = item->contents[c].aid;
                        f.dst = i;
                        f.freq = 0;
                        set.add(f);
                    }
                }
            }

            prev = cur;
        }

        logging_progress(&lg, s * 100 / N);
    }
    logging_progress_end(&lg);

    /* Generate edge features representing all pairs of labels.
       These features are not unobserved in the training data
       (zero expexcations). */
    if (connect_all_edges) {
        for (i = 0;i < L;++i) {
            for (j = 0;j < L;++j) {
                f.type = FT_TRANS;
                f.src = i;
                f.dst = j;
                f.freq = 0;
                set.add(f);
            }
        }
    }

    /* Convert the feature set to an feature array. */
    set.featureset_generate(features,  minfreq);
}

int crf1df_init_references(
    std::vector<feature_refs_t>& attributes,
    std::vector<feature_refs_t>& trans,
    const std::vector<crf1df_feature_t> &features,
    const int K,
    const int A,
    const int L
    )
{
    printf("generate: attrs: %lld, A = %d, trans: %lld, L = %d, features: %lld, K = %d\n", attributes.size(), A, trans.size(), L, features.size(), K);
    /*
        The purpose of this routine is to collect references (indices) of:
        - state features fired by each attribute (attributes)
        - transition features pointing from each label (trans)
    */
    /*
        Firstly, loop over the features to count the number of references.
        We don't use realloc() to avoid memory fragmentation.
     */
    for (int k = 0;k < K;++k) {
        const crf1df_feature_t *f = &features[k];
        switch (f->type) {
        case FT_STATE:
            attributes[f->src].num_features++;
            attributes[f->src].fids.push_back(k);
            break;
        case FT_TRANS:
            trans[f->src].num_features++;
            trans[f->src].fids.push_back(k);
            break;
        }
    }

    /*
        Secondarily, allocate memory blocks to store the feature references.
        We also clear fl->num_features fields, which will be used as indices
        in the next phase.
     */
    // for (int i = 0;i < A;++i) {
    //     feature_refs_t *fl = &attributes[i];
    //     fl->fids = (int*)calloc(fl->num_features, sizeof(int));
    //     fl->num_features = 0;
    // }
    // for (int i = 0;i < L;++i) {
    //     feature_refs_t *fl = &trans[i];
    //     fl->fids = (int*)calloc(fl->num_features, sizeof(int));
    //     fl->num_features = 0;
    // }

    /*
        Finally, store the feature indices.
     */
    // feature_refs_t *fl = NULL;
    // for (int k = 0;k < K;++k) {
    //     const crf1df_feature_t *f = &features[k];
    //     switch (f->type) {
    //     case FT_STATE:
    //         fl = &attributes[f->src];
    //         if (fl->fidv[fl->num_features] != k) {
    //             printf("not eq, v = %d, k = %d\n", fl->fidv[fl->num_features], k);
    //         }
    //         fl->fids[fl->num_features++] = k;
    //         break;
    //     case FT_TRANS:
    //         fl = &trans[f->src];
    //         if (fl->fidv[fl->num_features] != k) {
    //             printf("not eq, v = %d, k = %d\n", fl->fidv[fl->num_features], k);
    //             throw std::runtime_error("E");
    //         }
    //         fl->fids[fl->num_features++] = k;
    //         break;
    //     }
    // }
}
