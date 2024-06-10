/*
 *      Implementation of the training interface (crfsuite_trainer_t).
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

#include <stdlib.h>
#include <string.h>

#include <crfsuite.h>
#include "crfsuite_internal.h"
#include "params.h"
#include "logging.h"
#include "crf1d.h"

tag_crfsuite_train_internal::tag_crfsuite_train_internal(int ftype, int algorithm)
{
    this->lg = (logging_t*)calloc(1, sizeof(logging_t));
    this->m_params = params_create_instance();
    this->feature_type = ftype;
    this->algorithm = algorithm;

    this->gm = new tag_encoder();
    this->gm->exchange_options(this->m_params, 0);

    /* Initialize parameters for the training algorithm. */
    switch (algorithm) {
    case TRAIN_LBFGS:
        crfsuite_train_lbfgs_init(this->m_params);
        break;
    case TRAIN_L2SGD:
        crfsuite_train_l2sgd_init(this->m_params);
        break;
    case TRAIN_AVERAGED_PERCEPTRON:
        crfsuite_train_averaged_perceptron_init(this->m_params);
        break;
    case TRAIN_PASSIVE_AGGRESSIVE:
        crfsuite_train_passive_aggressive_init(this->m_params);
        break;
    case TRAIN_AROW:
        crfsuite_train_arow_init(this->m_params);
        break;
    }
}

tag_crfsuite_train_internal::~tag_crfsuite_train_internal()
{
    if (this->gm != NULL) {
        delete this->gm;
    }
    if (this->m_params != NULL) {
        this->m_params->release(this->m_params);
    }
    free(this->lg);
}

void tag_crfsuite_train_internal::set_message_callback(void *instance, crfsuite_logging_callback cbm)
{
    this->lg->func = cbm;
    this->lg->instance = instance;
}

crfsuite_params_t* tag_crfsuite_train_internal::params()
{
    crfsuite_params_t* params = this->m_params;
    params->addref(params);
    return params;
}

int tag_crfsuite_train_internal::train(const crfsuite_data_t *data, const char *filename, int holdout)
{
    char *algorithm = NULL;
    logging_t *lg = this->lg;
    encoder_t *gm = this->gm;
    floatval_t *w = NULL;
    dataset_t trainset;
    dataset_t testset;

    /* Prepare the data set(s) for training (and holdout evaluation). */
    trainset.init_trainset((crfsuite_data_t*)data, holdout);
    if (0 <= holdout) {
        testset.init_testset((crfsuite_data_t*)data, holdout);
        logging(lg, "Holdout group: %d\n", holdout+1);
        logging(lg, "\n");
    }

    /* Set the training set to the CRF, and generate features. */
    gm->exchange_options(this->m_params, -1);
    gm->initialize(&trainset, lg);

    /* Call the training algorithm. */
    switch (this->algorithm) {
    case TRAIN_LBFGS:
        crfsuite_train_lbfgs(
            gm,
            &trainset,
            (holdout != -1 ? &testset : NULL),
            this->m_params,
            lg,
            &w
            );
        break;
    case TRAIN_L2SGD:
        crfsuite_train_l2sgd(
            gm,
            &trainset,
            (holdout != -1 ? &testset : NULL),
            this->m_params,
            lg,
            &w
            );
        break;
    case TRAIN_AVERAGED_PERCEPTRON:
        crfsuite_train_averaged_perceptron(
            gm,
            &trainset,
            (holdout != -1 ? &testset : NULL),
            this->m_params,
            lg,
            &w
            );
        break;
    case TRAIN_PASSIVE_AGGRESSIVE:
        crfsuite_train_passive_aggressive(
            gm,
            &trainset,
            (holdout != -1 ? &testset : NULL),
            this->m_params,
            lg,
            &w
            );
        break;
    case TRAIN_AROW:
        crfsuite_train_arow(
            gm,
            &trainset,
            (holdout != -1 ? &testset : NULL),
            this->m_params,
            lg,
            &w
            );
        break;
    }

    /* Store the model file. */
    if (filename != NULL && *filename != '\0') {
        gm->save_model(filename, w, lg);
    }

    // if (0 <= holdout) {
    //     dataset_finish(&testset);
    // }
    // dataset_finish(&trainset);
    free(w);

    return 0;
}

int crf1de_create_instance(const char *interface, void **ptr)
{
    int ftype = FTYPE_NONE;
    int algorithm = TRAIN_NONE;

    /* Check if the interface name begins with "train/". */
    if (strncmp(interface, "train/", 6) != 0) {
        return 1;
    }
    interface += 6;

    /* Obtain the feature type. */
    if (strncmp(interface, "crf1d/", 6) == 0) {
        ftype = FTYPE_CRF1D;
        interface += 6;
    } else {
        return 1;
    }

    /* Obtain the training algorithm. */
    if (strcmp(interface, "lbfgs") == 0) {
        algorithm = TRAIN_LBFGS;
    } else if (strcmp(interface, "l2sgd") == 0) {
        algorithm = TRAIN_L2SGD;
    } else if (strcmp(interface, "averaged-perceptron") == 0) {
        algorithm = TRAIN_AVERAGED_PERCEPTRON;
    } else if (strcmp(interface, "passive-aggressive") == 0) {
        algorithm = TRAIN_PASSIVE_AGGRESSIVE;
    } else if (strcmp(interface, "arow") == 0) {
        algorithm = TRAIN_AROW;
    } else {
        return 1;
    }
    if (ftype != FTYPE_NONE && algorithm != TRAIN_NONE) {
        *ptr = new tag_crfsuite_train_internal(ftype, algorithm);
        return 0;
    }
    return 1;
}
