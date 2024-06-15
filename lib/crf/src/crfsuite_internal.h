/*
 *      CRFsuite internal interface.
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

#ifndef __CRFSUITE_INTERNAL_H__
#define __CRFSUITE_INTERNAL_H__

#include <crfsuite.h>
#include "logging.h"

enum {
    FTYPE_NONE = 0,             /**< Unselected. */
    FTYPE_CRF1D,                /**< 1st-order tyad features. */
    FTYPE_CRF1T,                /**< 1st-order triad features. */
};

enum {
    TRAIN_NONE = 0,             /**< Unselected. */
    TRAIN_LBFGS,                /**< L-BFGS batch training. */
    TRAIN_L2SGD,                /**< Pegasos online training. */
    TRAIN_AVERAGED_PERCEPTRON,  /**< Averaged perceptron. */
    TRAIN_PASSIVE_AGGRESSIVE,
    TRAIN_AROW,
};

struct tag_crfsuite_train_internal;
typedef struct tag_crfsuite_train_internal crfsuite_train_internal_t;

struct tag_encoder;
typedef struct tag_encoder encoder_t;


typedef void (*crfsuite_encoder_features_on_path_callback)(void *instance, int fid, floatval_t value);

struct Algo {
    virtual int train(encoder_t *gm,dataset_t *trainset,dataset_t *testset,crfsuite_params_t *params,logging_t *lg,std::vector<floatval_t>& output) = 0;
};

/**
 * Internal data structure for 
 */
struct tag_crfsuite_train_internal: public tag_crfsuite_trainer {
    encoder_t *gm;      /** Interface to the graphical model. */
    crfsuite_params_t *m_params;       /**< Parameter interface. */
    logging_t* lg;              /**< Logging interface. */
    int feature_type;           /**< Feature type. */
    Algo* algo;              /**< Training algorithm. */

    tag_crfsuite_train_internal(int ftype, int algorithm);
    void set_message_callback(void *instance, crfsuite_logging_callback cbm);
    crfsuite_params_t* params();
    int train(const crfsuite_dataset_t *data, const char *filename, int holdout, const TextVectorization *attrs, const TextVectorization *labels);
};

/**
 * Interface for a graphical model.
 */
struct tag_encoder
{
    void *internal;

    const floatval_t *w;
    floatval_t scale;

    const crfsuite_instance_t *inst;
    int level;

    int num_features;
    int cap_items;

    /**
     * Exchanges options.
     *  @param  self        The encoder instance.
     *  @param  params      The parameter interface.
     *  @param  mode        The direction of parameter exchange.
     *  @return             A status code.
     */
    int exchange_options(crfsuite_params_t* params, int mode);

    /**
     * Initializes the encoder with a training data set.
     *  @param  self        The encoder instance.
     *  @param  ds          The data set for training.
     *  @param  lg          The logging interface.
     *  @return             A status code.
     */
    void set_data(dataset_t &ds, logging_t *lg);

    /**
     * Compute the objective value and gradients for the whole data set.
     *  @param  self        The encoder instance.
     *  @param  ds          The data set.
     *  @param  w           The feature weights.
     *  @param  f           The pointer to a floatval_t variable to which the
     *                      objective value is stored by this function.
     *  @param  g           The pointer to the array that receives gradients.
     *  @return             A status code.
     */
    void objective_and_gradients_batch(dataset_t &ds, const floatval_t *w, floatval_t *f, floatval_t *g);

    void features_on_path(const crfsuite_instance_t *inst, const std::vector<int>& path, crfsuite_encoder_features_on_path_callback func, void *instance);



    /* Instance-wise operations. */
    void set_instance(const crfsuite_instance_t *inst);

    /* Level 0. */

    /* Level 1 (feature weights). */
    floatval_t score(const std::vector<int>& path);
    floatval_t viterbi(std::vector<int>& path);

    /* Level 2 (forward-backward). */
    floatval_t partition_factor();

    /* Level 3 (marginals). */
    floatval_t objective_and_gradients(floatval_t *g, floatval_t gain, floatval_t weight);

public:
    tag_encoder();
    ~tag_encoder();

    void save_model(const char *filename, const std::vector<floatval_t> &w, const TextVectorization *attrs, const TextVectorization *labels, logging_t *lg);
    /**
     * Sets the feature weights (and their scale factor).
     *  @param  self        The encoder instance.
     *  @param  w           The array of feature weights.
     *  @param  scale       The scale factor that should be applied to the
     *                      feature weights.
     *  @return             A status code.
     */
    void set_weights(const floatval_t *w, floatval_t scale);
    void set_level(int level);
    void holdout_evaluation(dataset_t *ds, const floatval_t *w, logging_t *lg);
};


void holdout_evaluation(
    encoder_t *gm,
    dataset_t *testset,
    const floatval_t *w,
    logging_t *lg
    );

Algo* create_lbfgs_algo(crfsuite_params_t* params);

void crfsuite_train_averaged_perceptron_init(crfsuite_params_t* params);

int crfsuite_train_averaged_perceptron(
    encoder_t *gm,
    dataset_t *trainset,
    dataset_t *testset,
    crfsuite_params_t *params,
    logging_t *lg,
    std::vector<floatval_t>& w
    );

void crfsuite_train_l2sgd_init(crfsuite_params_t* params);

int crfsuite_train_l2sgd(
    encoder_t *gm,
    dataset_t *trainset,
    dataset_t *testset,
    crfsuite_params_t *params,
    logging_t *lg,
    std::vector<floatval_t>& w
    );

void crfsuite_train_passive_aggressive_init(crfsuite_params_t* params);

int crfsuite_train_passive_aggressive(
    encoder_t *gm,
    dataset_t *trainset,
    dataset_t *testset,
    crfsuite_params_t *params,
    logging_t *lg,
    std::vector<floatval_t>& w
    );

void crfsuite_train_arow_init(crfsuite_params_t* params);

int crfsuite_train_arow(
    encoder_t *gm,
    dataset_t *trainset,
    dataset_t *testset,
    crfsuite_params_t *params,
    logging_t *lg,
    std::vector<floatval_t>& w
    );

int crf1de_create_instance(const char *iid, void **ptr);
int crf1m_create_instance_from_file(const char *filename, void **ptr);
int crf1m_create_instance_from_memory(const void *data, size_t size, void **ptr);


#endif/*__CRFSUITE_INTERNAL_H__*/
