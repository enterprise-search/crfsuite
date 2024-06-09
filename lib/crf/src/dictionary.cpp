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

class T: tag_crfsuite_dictionary {
public:
    T() {}
    virtual int addref()
    {
        return 0;
    }
    int release()
    {
        return 0;
    }
    int get(const char *str)
    {
        auto s = std::string(str);
        auto it = this->m.find(s);
        if (it != this->m.end()) {
            return it->second;
        } else {
            this->m[s] = this->v.size();
            this->v.push_back(s);
            return this->v.size()-1;
        }
        return 0;
    }
     int to_id(const char *str)
    {
        auto s = std::string(str);
        auto it = this->m.find(s);
        if (it != this->m.end()) {
            return it->second;
        }
        return -1;
    }

     int to_string(int id, char const **pstr)
    {
        if (id < this->v.size()) {
            auto s = this->v[id];
            char *dst = (char*)malloc(s.length()+1);
            if (dst) {
                strcpy(dst, s.c_str());
                *pstr = dst;
                return 0;
            }
        }
        return 1;
    }
    int num()
    {
        return this->v.size();
    }

    void free(const char *str)
    {
        // free((char*)str);
    }
private:
    std::map<std::string, int> m;
    std::vector<std::string> v;
};

int crfsuite_dictionary_create_instance(const char *interface, void **ptr)
{
    
    if (strcmp(interface, "dictionary") == 0) {
        auto p = new T();        
        *ptr = p;
        return 0;
    } else {
        return 1;
    }
}
