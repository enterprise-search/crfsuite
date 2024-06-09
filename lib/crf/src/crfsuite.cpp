/*
 *      CRFsuite library.
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

#include <os.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <crfsuite.h>
#include "crfsuite_internal.h"
#include "logging.h"

int crf1de_create_instance(const char *iid, void **ptr);
int crfsuite_dictionary_create_instance(const char *interface, void **ptr);
int crf1m_create_instance_from_file(const char *filename, void **ptr);
int crf1m_create_instance_from_memory(const void *data, size_t size, void **ptr);

int crfsuite_create_instance(const char *iid, void **ptr)
{
    int ret = 
        crf1de_create_instance(iid, ptr) == 0 ||
        crfsuite_dictionary_create_instance(iid, ptr) == 0;

    return ret;
}

int crfsuite_create_instance_from_file(const char *filename, void **ptr)
{
    int ret = crf1m_create_instance_from_file(filename, ptr);
    return ret;
}

int crfsuite_create_instance_from_memory(const void *data, size_t size, void **ptr)
{
    int ret = crf1m_create_instance_from_memory(data, size, ptr);
    return ret;
}

static char *safe_strncpy(char *dst, const char *src, size_t n)
{
    strncpy(dst, src, n-1);
    dst[n-1] = 0;
    return dst;
}

void crfsuite_evaluation_init(crfsuite_evaluation_t* eval, int n)
{
    memset(eval, 0, sizeof(*eval));
    eval->tbl = (crfsuite_label_evaluation_t*)calloc(n+1, sizeof(crfsuite_label_evaluation_t));
    if (eval->tbl != NULL) {
        eval->num_labels = n;
    }
}

void crfsuite_evaluation_clear(crfsuite_evaluation_t* eval)
{
    int i;
    for (i = 0;i <= eval->num_labels;++i) {
        memset(&eval->tbl[i], 0, sizeof(eval->tbl[i]));
    }

    eval->item_total_correct = 0;
    eval->item_total_num = 0;
    eval->item_total_model = 0;
    eval->item_total_observation = 0;
    eval->item_accuracy = 0;

    eval->inst_total_correct = 0;
    eval->inst_total_num = 0;
    eval->inst_accuracy = 0;

    eval->macro_precision = 0;
    eval->macro_recall = 0;
    eval->macro_fmeasure = 0;
}

void crfsuite_evaluation_finish(crfsuite_evaluation_t* eval)
{
    free(eval->tbl);
    memset(eval, 0, sizeof(*eval));
}

int crfsuite_evaluation_accmulate(crfsuite_evaluation_t* eval, const std::vector<int>& reference, const std::vector<int>& prediction, int T)
{
    int t, nc = 0;

    for (t = 0;t < T;++t) {
        int lr = reference[t];
        int lt = prediction[t];

        if (eval->num_labels <= lr || eval->num_labels <= lt) {
            return 1;
        }

        ++eval->tbl[lr].num_observation;
        ++eval->tbl[lt].num_model;
        if (lr == lt) {
            ++eval->tbl[lr].num_correct;
            ++nc;
        }
        ++eval->item_total_num;
    }

    if (nc == T) {
        ++eval->inst_total_correct;
    }
    ++eval->inst_total_num;

    return 0;
}

void crfsuite_evaluation_finalize(crfsuite_evaluation_t* eval)
{
    int i;

    for (i = 0;i <= eval->num_labels;++i) {
        crfsuite_label_evaluation_t* lev = &eval->tbl[i];

        /* Do not evaluate labels that does not in the test data. */
        if (lev->num_observation == 0) {
            continue;
        }

        /* Sum the number of correct labels for accuracy calculation. */
        eval->item_total_correct += lev->num_correct;
        eval->item_total_model += lev->num_model;
        eval->item_total_observation += lev->num_observation;

        /* Initialize the precision, recall, and f1-measure values. */
        lev->precision = 0;
        lev->recall = 0;
        lev->fmeasure = 0;

        /* Compute the precision, recall, and f1-measure values. */
        if (lev->num_model > 0) {
            lev->precision = lev->num_correct / (double)lev->num_model;
        }
        if (lev->num_observation > 0) {
            lev->recall = lev->num_correct / (double)lev->num_observation;
        }
        if (lev->precision + lev->recall > 0) {
            lev->fmeasure = lev->precision * lev->recall * 2 / (lev->precision + lev->recall);
        }

        /* Exclude unknown labels from calculation of macro-average values. */
        if (i != eval->num_labels) {
            eval->macro_precision += lev->precision;
            eval->macro_recall += lev->recall;
            eval->macro_fmeasure += lev->fmeasure;
        }
    }

    /* Copute the macro precision, recall, and f1-measure values. */
    eval->macro_precision /= eval->num_labels;
    eval->macro_recall /= eval->num_labels;
    eval->macro_fmeasure /= eval->num_labels;

    /* Compute the item accuracy. */
    eval->item_accuracy = 0;
    if (0 < eval->item_total_num) {
        eval->item_accuracy = eval->item_total_correct / (double)eval->item_total_num;
    }

    /* Compute the instance accuracy. */
    eval->inst_accuracy = 0;
    if (0 < eval->inst_total_num) {
        eval->inst_accuracy = eval->inst_total_correct / (double)eval->inst_total_num;
    }
}

void crfsuite_evaluation_output(crfsuite_evaluation_t* eval, crfsuite_dictionary_t* labels, crfsuite_logging_callback cbm, void *instance)
{
    int i;
    const char *lstr = NULL;
    logging_t lg;

    lg.func = cbm;
    lg.instance = instance;

    logging(&lg, "Performance by label (#match, #model, #ref) (precision, recall, F1):\n");

    for (i = 0;i < eval->num_labels;++i) {
        const crfsuite_label_evaluation_t* lev = &eval->tbl[i];

        labels->to_string(i, &lstr);
        if (lstr == NULL) lstr = "[UNKNOWN]";

        if (lev->num_observation == 0) {
            logging(&lg, "    %s: (%d, %d, %d) (******, ******, ******)\n",
                lstr, lev->num_correct, lev->num_model, lev->num_observation
                );
        } else {
            logging(&lg, "    %s: (%d, %d, %d) (%1.4f, %1.4f, %1.4f)\n",
                lstr, lev->num_correct, lev->num_model, lev->num_observation,
                lev->precision, lev->recall, lev->fmeasure
                );
        }
        labels->free( lstr);
    }
    logging(&lg, "Macro-average precision, recall, F1: (%f, %f, %f)\n",
        eval->macro_precision, eval->macro_recall, eval->macro_fmeasure
        );
    logging(&lg, "Item accuracy: %d / %d (%1.4f)\n",
        eval->item_total_correct, eval->item_total_num, eval->item_accuracy
        );
    logging(&lg, "Instance accuracy: %d / %d (%1.4f)\n",
        eval->inst_total_correct, eval->inst_total_num, eval->inst_accuracy
        );
}

int crfsuite_interlocked_increment(int *count)
{
    return ++(*count);
}

int crfsuite_interlocked_decrement(int *count)
{
    return --(*count);
}
