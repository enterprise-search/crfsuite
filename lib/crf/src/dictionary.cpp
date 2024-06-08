/*
 *      Implementation of dictionary.
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

#include <stdlib.h>
#include <string.h>
#include <crfsuite.h>
#include <map>
#include <vector>
#include <string>

class T {
    public:
    std::map<std::string, int> m;
    std::vector<std::string> v;
};

static int dictionary_addref(crfsuite_dictionary_t* dic)
{
    // return crfsuite_interlocked_increment(&dic->nref);
    return 0;
}

static int dictionary_release(crfsuite_dictionary_t* dic)
{
    return 0;
    // int count = crfsuite_interlocked_decrement(&dic->nref);
    // if (count == 0) {
    //     quark_t *qrk = (quark_t*)dic->internal;
    //     quark_delete(qrk);
    //     free(dic);
    // }
    // return count;
}

static int dictionary_get(crfsuite_dictionary_t* dic, const char *str)
{
    T *t = (T*)dic->internal;
    auto s = std::string(str);
    auto it = t->m.find(s);
    if (it != t->m.end()) {
        return it->second;
    } else {
        t->m[s] = t->v.size();
        t->v.push_back(s);
        return t->v.size()-1;
    }
    return 0;
}

static int dictionary_to_id(crfsuite_dictionary_t* dic, const char *str)
{
    T *t = (T*)dic->internal;
    auto s = std::string(str);
    auto it = t->m.find(s);
    if (it != t->m.end()) {
        return it->second;
    }
    return -1;
}

static int dictionary_to_string(crfsuite_dictionary_t* dic, int id, char const **pstr)
{
    T *t = (T*)dic->internal;
    if (id < t->v.size()) {
        auto s = t->v[id];
        char *dst = (char*)malloc(s.length()+1);
        if (dst) {
            strcpy(dst, s.c_str());
            *pstr = dst;
            return 0;
        }
    }
    return 1;
}

static int dictionary_num(crfsuite_dictionary_t* dic)
{
    T* t = (T*)dic->internal;
    return t->v.size();
}

static void dictionary_free(crfsuite_dictionary_t* dic, const char *str)
{
    // free((char*)str);
}

int crfsuite_dictionary_create_instance(const char *interface, void **ptr)
{
    if (strcmp(interface, "dictionary") == 0) {
        crfsuite_dictionary_t* dic = (crfsuite_dictionary_t*)calloc(1, sizeof(crfsuite_dictionary_t));
        if (dic != NULL) {
            dic->internal = new T();
            dic->nref = 1;
            dic->addref = dictionary_addref;
            dic->release = dictionary_release;
            dic->get = dictionary_get;
            dic->to_id = dictionary_to_id;
            dic->to_string = dictionary_to_string;
            dic->num = dictionary_num;
            dic->free = dictionary_free;
            *ptr = dic;
            return 0;
        } else {
            return -1;
        }
    } else {
        return 1;
    }
}
