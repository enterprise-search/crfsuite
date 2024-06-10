/*
 *      The 1st-order linear-chain CRF with dyad features (CRF1d).
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

#ifndef    __CRF1D_H__
#define    __CRF1D_H__

#include <crfsuite.h>
#include <cqdb.h>
#include "crfsuite_internal.h"
#include <vector>


/**
 * \defgroup crf1d_context.c
 */
/** @{ */

/**
 * Functionality flags for contexts.
 *  @see    crf1dc_new().
 */
enum {
    CTXF_BASE       = 0x01,
    CTXF_VITERBI    = 0x01,
    CTXF_MARGINALS  = 0x02,
    CTXF_ALL        = 0xFF,
};

/**
 * Reset flags.
 *  @see    crf1dc_reset().
 */
enum {
    RF_STATE    = 0x01,     /**< Reset state scores. */
    RF_TRANS    = 0x02,     /**< Reset transition scores. */
    RF_ALL      = 0xFF,     /**< Reset all. */
};

/**
 * Context structure.
 *  This structure maintains internal data for an instance.
 */
struct crf1d_context_t {
    /**
     * Flag specifying the functionality.
     */
    int flag;

    /**
     * The total number of distinct labels (L).
     */
    int num_labels;

    /**
     * The number of items (T) in the instance.
     */
    int num_items;

    /**
     * The maximum number of labels.
     */
    int cap_items;

    /**
     * Logarithm of the normalization factor for the instance.
     *  This is equivalent to the total scores of all paths in the lattice.
     */
    floatval_t log_norm;

    /**
     * State scores.
     *  This is a [T][L] matrix whose element [t][l] presents total score
     *  of state features associating label #l at #t.
     */
    std::vector<floatval_t> state;

    /**
     * Transition scores.
     *  This is a [L][L] matrix whose element [i][j] represents the total
     *  score of transition features associating labels #i and #j.
     */
    std::vector<floatval_t> trans;

    /**
     * Alpha score matrix.
     *  This is a [T][L] matrix whose element [t][l] presents the total
     *  score of paths starting at BOS and arraiving at (t, l).
     */
    std::vector<floatval_t> alpha_score;

    /**
     * Beta score matrix.
     *  This is a [T][L] matrix whose element [t][l] presents the total
     *  score of paths starting at (t, l) and arraiving at EOS.
     */
    std::vector<floatval_t> beta_score;

    /**
     * Scale factor vector.
     *  This is a [T] vector whose element [t] presents the scaling
     *  coefficient for the alpha_score and beta_score.
     */
    std::vector<floatval_t> scale_factor;

    /**
     * Row vector (work space).
     *  This is a [T] vector used internally for a work space.
     */
    std::vector<floatval_t> row;

    /**
     * Backward edges.
     *  This is a [T][L] matrix whose element [t][j] represents the label #i
     *  that yields the maximum score to arrive at (t, j).
     *  This member is available only with CTXF_VITERBI flag enabled.
     */
    std::vector<int> backward_edge;

    /**
     * Exponents of state scores.
     *  This is a [T][L] matrix whose element [t][l] presents the exponent
     *  of the total score of state features associating label #l at #t.
     *  This member is available only with CTXF_MARGINALS flag.
     */
    std::vector<floatval_t> exp_state;

    /**
     * Exponents of transition scores.
     *  This is a [L][L] matrix whose element [i][j] represents the exponent
     *  of the total score of transition features associating labels #i and #j.
     *  This member is available only with CTXF_MARGINALS flag.
     */
    std::vector<floatval_t> exp_trans;

    /**
     * Model expectations of states.
     *  This is a [T][L] matrix whose element [t][l] presents the model
     *  expectation (marginal probability) of the state (t,l)
     *  This member is available only with CTXF_MARGINALS flag.
     */
    std::vector<floatval_t> mexp_state;

    /**
     * Model expectations of transitions.
     *  This is a [L][L] matrix whose element [i][j] presents the model
     *  expectation of the transition (i--j).
     *  This member is available only with CTXF_MARGINALS flag.
     */
    std::vector<floatval_t> mexp_trans;
        
public:
    crf1d_context_t(int flag, int L, int T) : flag(flag), num_labels(L), trans(L*L)
    {
        int ret = 0;

        if (this->flag & CTXF_MARGINALS) {
            this->exp_trans = std::vector<floatval_t>(L*L);
            this->mexp_trans = std::vector<floatval_t>(L*L);
        }

        crf1dc_set_num_items(T);
        /* T gives the 'hint' for maximum length of items. */
        this->num_items = 0;
    }
    floatval_t crf1dc_lognorm() const { return this->log_norm; }
    void crf1dc_set_num_items( int T);
    void crf1dc_reset( int flag);
    inline void crf1dc_exp_state()
    {
        std::transform(this->state.begin(), this->state.end(), this->exp_state.begin(), [](auto& x){return exp(x);});
    }
    void crf1dc_exp_transition();
    void crf1dc_alpha_score();
    void crf1dc_beta_score();
    void crf1dc_marginals();
    floatval_t crf1dc_marginal_point(crf1d_context_t *ctx, int l, int t);
    floatval_t crf1dc_marginal_path(crf1d_context_t *ctx, const int *path, int begin, int end);
    floatval_t crf1dc_score( const std::vector<int>& labels);
    floatval_t crf1dc_viterbi( std::vector<int>& labels);

    floatval_t crf1dc_marginal_point(int l, int t) const
    {
        floatval_t fwd = this->alpha_score[this->num_labels * t + l];
        floatval_t bwd = this->beta_score[this->num_labels * t + l];
        return fwd * bwd / this->scale_factor[t];
    }
    floatval_t crf1dc_marginal_path(const int *path, int begin, int end);
};

#define    MATRIX(p, xl, x, y)        ((p)[(xl) * (y) + (x)])

#define    ALPHA_SCORE(ctx, t) \
    (&MATRIX(ctx->alpha_score, ctx->num_labels, 0, t))
#define    BETA_SCORE(ctx, t) \
    (&MATRIX(ctx->beta_score, ctx->num_labels, 0, t))
#define    STATE_SCORE(ctx, i) \
    (&MATRIX(ctx->state, ctx->num_labels, 0, i))
#define    TRANS_SCORE(ctx, i) \
    (&MATRIX(ctx->trans, ctx->num_labels, 0, i))
#define    EXP_STATE_SCORE(ctx, i) \
    (&MATRIX(ctx->exp_state, ctx->num_labels, 0, i))
#define    EXP_TRANS_SCORE(ctx, i) \
    (&MATRIX(ctx->exp_trans, ctx->num_labels, 0, i))
#define    STATE_MEXP(ctx, i) \
    (&MATRIX(ctx->mexp_state, ctx->num_labels, 0, i))
#define    TRANS_MEXP(ctx, i) \
    (&MATRIX(ctx->mexp_trans, ctx->num_labels, 0, i))
#define    BACKWARD_EDGE_AT(ctx, t) \
    (&MATRIX(ctx->backward_edge, ctx->num_labels, 0, t))

void crf1dc_debug_context(FILE *fp);

/** @} */



/**
 * \defgroup crf1d_feature.c
 */
/** @{ */

/**
 * Feature type.
 */
enum {
    FT_STATE = 0,    /**< State features. */
    FT_TRANS,        /**< Transition features. */
};

/**
 * A feature (for either state or transition).
 */
 struct crf1df_feature_t {
    /**
     * Feature type.
     *    Possible values are:
     *    - FT_STATE (0) for state features.
     *    - FT_TRANS (1) for transition features.
     */
    int        type;

    /**
     * Source id.
     *    The semantic of this field depends on the feature type:
     *    - attribute id for state features (type == 0).
     *    - output label id for transition features (type != 0).
     */
    int        src;

    /**
     * Destination id.
     *    Label id emitted by this feature.
     */
    int        dst;

    /**
     * Frequency (observation expectation).
     */
    floatval_t    freq;
} ;

/**
 * Feature references.
 *    This is a collection of feature ids used for faster accesses.
 */
 struct feature_refs_t {
    int        num_features;    /**< Number of features referred */
    // int*    fids;            /**< Array of feature ids */
    std::vector<int> fids;
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
    );


int crf1df_init_references(
    std::vector<feature_refs_t>& attributes,
    std::vector<feature_refs_t>& trans,
    const std::vector<crf1df_feature_t>& features,
    const int K,
    const int A,
    const int L
    );

/** @} */



/**
 * \defgroup crf1d_model.c
 */
/** @{ */

struct tag_crf1dm;
typedef struct tag_crf1dm crf1dm_t;

 struct crf1dm_feature_t {
    int        type;
    int        src;
    int        dst;
    floatval_t weight;
} ;

 struct header_t{
    uint8_t     magic[4];       /* File magic. */
    uint32_t    size;           /* File size. */
    uint8_t     type[4];        /* Model type */
    uint32_t    version;        /* Version number. */
    uint32_t    num_features;   /* Number of features. */
    uint32_t    num_labels;     /* Number of labels. */
    uint32_t    num_attrs;      /* Number of attributes. */
    uint32_t    off_features;   /* Offset to features. */
    uint32_t    off_labels;     /* Offset to label CQDB. */
    uint32_t    off_attrs;      /* Offset to attribute CQDB. */
    uint32_t    off_labelrefs;  /* Offset to label feature references. */
    uint32_t    off_attrrefs;   /* Offset to attribute feature references. */
} ;

 struct featureref_header_t {
    uint8_t     chunk[4];       /* Chunk id */
    uint32_t    size;           /* Chunk size. */
    uint32_t    num;            /* Number of items. */
    uint32_t    offsets[1];     /* Offsets. */
} ;

 struct feature_header_t {
    uint8_t     chunk[4];       /* Chunk id */
    uint32_t    size;           /* Chunk size. */
    uint32_t    num;            /* Number of items. */
} ;

 /*
 *    Implementation of crfsuite_dictionary_t object for attributes.
 *    This object is instantiated only by a crfsuite_model_t object.
 */


 struct OneHotEncoder: crfsuite_dictionary_t {
 private:
     cqdb_t*        db;
     size_t n;
 public:
     OneHotEncoder(cqdb_t *db, size_t n): db(db), n(n) {}
     int get(const char *str)
     {
         /* This object is ready only. */
         throw std::runtime_error("supported");
     }

     int to_id(const char *str) { return cqdb_to_id(db, str); }

     int to_string(int id, char const **pstr)
     {
         *pstr = cqdb_to_string(db, id);
         return 0;
     }

     int num() { return this->n; }

     void free(const char *str)
     {
         /* all strings are freed on the release of the dictionary object. */
     }
};

 /*
 *    Implementation of crfsuite_model_t object.
 *    This object is instantiated by crf1m_model_create() function.
 */

struct tag_crf1dm: tag_crfsuite_model {
    uint8_t*       buffer_orig;
    const uint8_t* buffer;
    uint32_t       size;
    header_t*      header;
    cqdb_t*        labels;
    cqdb_t*        attrs;

private:
    std::vector<feature_refs_t> attr_refs;
    std::vector<feature_refs_t> label_refs;
public:
    tag_crf1dm(const char *filename);
    tag_crf1dm(const void *data, size_t size) : tag_crf1dm(NULL, (const uint8_t*)data, size) {}
    tag_crf1dm(uint8_t* buffer_orig, const uint8_t* buffer, uint32_t size);
    ~tag_crf1dm();

    int crf1dm_get_num_attrs();
    int crf1dm_get_num_labels();
    const char *crf1dm_to_label(int lid);
    int crf1dm_to_lid(const char *value);
    int crf1dm_to_aid(const char *value);
    const char *crf1dm_to_attr(int aid);
    int crf1dm_get_labelref(int lid, feature_refs_t* ref);
    int crf1dm_get_attrref(int aid, feature_refs_t* ref);
    int crf1dm_get_featureid(feature_refs_t* ref, int i);
    int crf1dm_get_feature(int fid, crf1dm_feature_t* f);
    void dump(FILE *fp);
public:

    crfsuite_tagger_t* get_tagger();
    crfsuite_dictionary_t* get_labels()
    {
        /* We don't increment the reference counter. */
        return new OneHotEncoder(labels, header->num_labels);
    }

    crfsuite_dictionary_t* get_attrs()
    {
        /* We don't increment the reference counter. */
        return new OneHotEncoder(attrs, header->num_attrs);
    }
};

struct tag_crf1dmw {
    FILE *fp;
    int state;
    header_t header;
    cqdb_writer_t* dbw;
    featureref_header_t* href;
    feature_header_t* hfeat;

    tag_crf1dmw(const char *filename);
    ~tag_crf1dmw();
    int crf1dmw_open_labels(int num_labels);
    int crf1dmw_close_labels();
    int crf1dmw_put_label(int lid, const char *value);
    int crf1dmw_open_attrs(int num_attributes);
    int crf1dmw_close_attrs();
    int crf1dmw_put_attr(int aid, const char *value);
    int crf1dmw_open_labelrefs(int num_labels);
    int crf1dmw_close_labelrefs();
    int crf1dmw_put_labelref(int lid, const feature_refs_t* ref, int *map);
    int crf1dmw_open_attrrefs(int num_attrs);
    int crf1dmw_close_attrrefs();
    int crf1dmw_put_attrref(int aid, const feature_refs_t* ref, int *map);
    int crf1dmw_open_features();
    int crf1dmw_close_features();
    int crf1dmw_put_feature(int fid, const crf1dm_feature_t* f);
};

/** @} */

struct crf1dt_t : tag_crfsuite_tagger {

    crf1dm_t *model;        /**< CRF model. */
    crf1d_context_t *ctx;   /**< CRF context. */
    int num_labels;         /**< Number of distinct output labels (L). */
    int num_attributes;     /**< Number of distinct attributes (A). */
    int level;
public:
    crf1dt_t(crf1dm_t* crf1dm);
    void crf1dt_state_score(const crfsuite_instance_t *inst);
    void crf1dt_transition_score();
    void crf1dt_set_level(int level);
    void crf1dt_delete();
public: // interface
    /*
     *    Implementation of crfsuite_tagger_t object.
     *    This object is instantiated only by a crfsuite_model_t object.
     */
    int length() const { return this->ctx->num_items; }
    floatval_t viterbi(std::vector<int>& labels) { return this->ctx->crf1dc_viterbi(labels); }
    floatval_t score(std::vector<int>& path) { return this->ctx->crf1dc_score(path); }
    int set(crfsuite_instance_t *inst);
    floatval_t lognorm();
    floatval_t marginal_point( int l, int t);
    floatval_t marginal_path( const int *path, int begin, int end);
};

#endif/*__CRF1D_H__*/
