/*
 *        Data reader.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <crfsuite.h>
#include "iwa.h"

static int progress(FILE *fpo, int prev, int current)
{
    while (prev < current) {
        ++prev;
        if (prev % 2 == 0) {
            if (prev % 10 == 0) {
                fprintf(fpo, "%d", prev / 10);
                fflush(fpo);
            } else {
                fprintf(fpo, ".");
                fflush(fpo);
            }
        }
    }
    return prev;
}

dataset_t read_data(TextVectorization* attrs, TextVectorization*labels, FILE *fpi, FILE *fpo, int group)
{
    int n = 0;
    int lid = -1;
    crfsuite_item_t item;
    crfsuite_attribute_t cont;
    iwa_t* iwa = NULL;
    const iwa_token_t *token = NULL;
    long filesize = 0, begin = 0, offset = 0;
    int prev = 0, current = 0;
    std::vector<crfsuite_instance_t> instances;

    /* Initialize the instance.*/
    crfsuite_instance_t inst;
    inst.group = group;

    /* Obtain the file size. */
    begin = ftell(fpi);
    fseek(fpi, 0, SEEK_END);
    filesize = ftell(fpi) - begin;
    fseek(fpi, begin, SEEK_SET);

    /* */
    fprintf(fpo, "0");
    fflush(fpo);
    prev = 0;

    iwa = iwa_reader(fpi);
    while (token = iwa_read(iwa), token != NULL) {
        /* Progress report. */
        offset = ftell(fpi);
        current = (int)((offset - begin) * 100.0 / (double)filesize);
        prev = progress(fpo, prev, current);

        switch (token->type) {
        case IWA_BOI:
            /* Initialize an item. */
            lid = -1;
            item.contents.clear();
            break;
        case IWA_EOI:
            /* Append the item to the instance. */
            if (0 <= lid) {
                inst.append(item, lid);
            }
            item.contents.clear();
            break;
        case IWA_ITEM:
            if (lid == -1) {
                if (strncmp(token->attr, "@", 1) == 0) {
                    /* Declaration. */
                    if (strcmp(token->attr, "@weight") == 0) {
                        /* Instance weighting. */
                        inst.weight = atof(token->value);
                    } else {
                        /* Unrecognized declaration. */
                        fprintf(fpo, "\n");
                        fprintf(fpo, "ERROR: unrecognized declaration: %s\n", token->attr);
                        iwa_delete(iwa);
                        throw std::runtime_error("read");
                    }
                } else {
                    /* Label. */
                    lid = labels->get(token->attr);
                }
            } else {
                cont.aid = attrs->get(token->attr);
                if (token->value && *token->value) {
                    cont.value = atof(token->value);
                } else {
                    cont.value = 1.0;
                }
                item.append(cont);
            }
            break;
        case IWA_NONE:
        case IWA_EOF:
            /* Put the training instance. */
            if (!inst.empty()) {
                instances.push_back(inst);
            }

            inst.clear();
            inst.group = group;
            inst.weight = 1.;
            ++n;
            break;
        }
    }

    progress(fpo, prev, 100);
    fprintf(fpo, "\n");

    iwa_delete(iwa);

    return dataset_t(instances, labels->num(), attrs->num());
}
