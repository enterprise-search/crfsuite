/*
 *      CRF1d model.
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

#include "os.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cqdb.h>

#include <crfsuite.h>
#include "crf1d.h"

#define FILEMAGIC       "lCRF"
#define MODELTYPE       "FOMC"
#define VERSION_NUMBER  (100)
#define CHUNK_LABELREF  "LFRF"
#define CHUNK_ATTRREF   "AFRF"
#define CHUNK_FEATURE   "FEAT"
#define HEADER_SIZE     48
#define CHUNK_SIZE      12
#define FEATURE_SIZE    20

enum {
    WSTATE_NONE,
    WSTATE_LABELS,
    WSTATE_ATTRS,
    WSTATE_LABELREFS,
    WSTATE_ATTRREFS,
    WSTATE_FEATURES,
};

enum {
    KT_GLOBAL = 'A',
    KT_NUMATTRS,
    KT_NUMLABELS,
    KT_STR2LID,
    KT_LID2STR,
    KT_STR2AID,
    KT_FEATURE,
};

static int write_uint8(FILE *fp, uint8_t value)
{
    return fwrite(&value, sizeof(value), 1, fp) == 1 ? 0 : 1;
}

static int read_uint8(const uint8_t* buffer, uint8_t* value)
{
    *value = *buffer;
    return sizeof(*value);
}

static int write_uint32(FILE *fp, uint32_t value)
{
    uint8_t buffer[4];
    buffer[0] = (uint8_t)(value & 0xFF);
    buffer[1] = (uint8_t)(value >> 8);
    buffer[2] = (uint8_t)(value >> 16);
    buffer[3] = (uint8_t)(value >> 24);
    return fwrite(buffer, sizeof(uint8_t), 4, fp) == 4 ? 0 : 1;
}

static int read_uint32(const uint8_t* buffer, uint32_t* value)
{
    *value  = ((uint32_t)buffer[0]);
    *value |= ((uint32_t)buffer[1] << 8);
    *value |= ((uint32_t)buffer[2] << 16);
    *value |= ((uint32_t)buffer[3] << 24);
    return sizeof(*value);
}

static int write_uint8_array(FILE *fp, uint8_t *array, size_t n)
{
    size_t i;
    int ret = 0;
    for (i = 0;i < n;++i) {
        ret |= write_uint8(fp, array[i]);
    }
    return ret;
}

static int read_uint8_array(const uint8_t* buffer, uint8_t *array, size_t n)
{
    size_t i;
    int ret = 0;
    for (i = 0;i < n;++i) {
        int size = read_uint8(buffer, &array[i]);
        buffer += size;
        ret += size;
    }
    return ret;
}

static void write_float(FILE *fp, floatval_t value)
{
    /*
        We assume:
            - sizeof(floatval_t) = sizeof(double) = sizeof(uint64_t)
            - the byte order of floatval_t and uint64_t is the same
            - ARM's mixed-endian is not supported
    */
    uint64_t iv;
    uint8_t buffer[8];

    /* Copy the memory image of floatval_t value to uint64_t. */
    memcpy(&iv, &value, sizeof(iv));

    buffer[0] = (uint8_t)(iv & 0xFF);
    buffer[1] = (uint8_t)(iv >> 8);
    buffer[2] = (uint8_t)(iv >> 16);
    buffer[3] = (uint8_t)(iv >> 24);
    buffer[4] = (uint8_t)(iv >> 32);
    buffer[5] = (uint8_t)(iv >> 40);
    buffer[6] = (uint8_t)(iv >> 48);
    buffer[7] = (uint8_t)(iv >> 56);
    fwrite(buffer, sizeof(uint8_t), 8, fp);
}

static int read_float(const uint8_t* buffer, floatval_t* value)
{
    uint64_t iv;
    iv  = ((uint64_t)buffer[0]);
    iv |= ((uint64_t)buffer[1] << 8);
    iv |= ((uint64_t)buffer[2] << 16);
    iv |= ((uint64_t)buffer[3] << 24);
    iv |= ((uint64_t)buffer[4] << 32);
    iv |= ((uint64_t)buffer[5] << 40);
    iv |= ((uint64_t)buffer[6] << 48);
    iv |= ((uint64_t)buffer[7] << 56);
    memcpy(value, &iv, sizeof(*value));
    return sizeof(*value);
}

tag_crf1dmw::tag_crf1dmw(const char *filename)
{
    header_t *header = NULL;
    /* Open the file for writing. */
    this->fp = fopen(filename, "wb");
    if (this->fp == NULL) {
        printf("ERROR fopen\n");
    }

    /* Fill the members in the header. */
    header = &this->header;
    memcpy(header->magic, FILEMAGIC, 4);
    memcpy(header->type, MODELTYPE, 4);
    header->version = VERSION_NUMBER;

    /* Advance the file position to skip the file header. */
    if (fseek(this->fp, HEADER_SIZE, SEEK_CUR) != 0) {
        printf("ERR fseek\n");
    }
}

tag_crf1dmw::~tag_crf1dmw()
{
    FILE *fp = this->fp;
    header_t *header = &this->header;

    /* Store the file size. */
    header->size = (uint32_t)ftell(fp);

    /* Move the file position to the head. */
    if (fseek(fp, 0, SEEK_SET) != 0) {
        printf("EE fseek\n");
    }

    /* Write the file header. */
    write_uint8_array(fp, header->magic, sizeof(header->magic));
    write_uint32(fp, header->size);
    write_uint8_array(fp, header->type, sizeof(header->type));
    write_uint32(fp, header->version);
    write_uint32(fp, header->num_features);
    write_uint32(fp, header->num_labels);
    write_uint32(fp, header->num_attrs);
    write_uint32(fp, header->off_features);
    write_uint32(fp, header->off_labels);
    write_uint32(fp, header->off_attrs);
    write_uint32(fp, header->off_labelrefs);
    write_uint32(fp, header->off_attrrefs);

    /* Check for any error occurrence. */
    if (ferror(fp)) {
        printf("ERR ferror\n");
    }
    /* Close the writer. */
    fclose(fp);
}

int tag_crf1dmw::crf1dmw_open_labels(int num_labels)
{
    /* Check if we aren't writing anything at this moment. */
    if (this->state != WSTATE_NONE) {
        return 1;
    }

    /* Store the current offset. */
    this->header.off_labels = (uint32_t)ftell(this->fp);

    /* Open a CQDB chunk for writing. */
    this->dbw = cqdb_writer(this->fp, 0);
    if (this->dbw == NULL) {
        this->header.off_labels = 0;
        return 1;
    }

    this->state = WSTATE_LABELS;
    this->header.num_labels = num_labels;
    return 0;
}

int tag_crf1dmw::crf1dmw_close_labels()
{
    /* Make sure that we are writing labels. */
    if (this->state != WSTATE_LABELS) {
        return 1;
    }

    /* Close the CQDB chunk. */
    if (cqdb_writer_close(this->dbw)) {
        return 1;
    }

    this->dbw = NULL;
    this->state = WSTATE_NONE;
    return 0;
}

int tag_crf1dmw::crf1dmw_put_label(int lid, const char *value)
{
    /* Make sure that we are writing labels. */
    if (this->state != WSTATE_LABELS) {
        return 1;
    }

    /* Put the label. */
    if (cqdb_writer_put(this->dbw, value, lid)) {
        return 1;
    }

    return 0;
}

int tag_crf1dmw::crf1dmw_open_attrs(int num_attrs)
{
    /* Check if we aren't writing anything at this moment. */
    if (this->state != WSTATE_NONE) {
        return 1;
    }

    /* Store the current offset. */
    this->header.off_attrs = (uint32_t)ftell(this->fp);

    /* Open a CQDB chunk for writing. */
    this->dbw = cqdb_writer(this->fp, 0);
    if (this->dbw == NULL) {
        this->header.off_attrs = 0;
        return 1;
    }

    this->state = WSTATE_ATTRS;
    this->header.num_attrs = num_attrs;
    return 0;
}

int tag_crf1dmw::crf1dmw_close_attrs()
{
    /* Make sure that we are writing attributes. */
    if (this->state != WSTATE_ATTRS) {
        return 1;
    }

    /* Close the CQDB chunk. */
    if (cqdb_writer_close(this->dbw)) {
        return 1;
    }

    this->dbw = NULL;
    this->state = WSTATE_NONE;
    return 0;
}

int tag_crf1dmw::crf1dmw_put_attr(int aid, const char *value)
{
    /* Make sure that we are writing labels. */
    if (this->state != WSTATE_ATTRS) {
        return 1;
    }

    /* Put the attribute. */
    if (cqdb_writer_put(this->dbw, value, aid)) {
        return 1;
    }

    return 0;
}

int tag_crf1dmw::crf1dmw_open_labelrefs(int num_labels)
{
    tag_crf1dmw* writer = this;
    uint32_t offset;
    FILE *fp = writer->fp;
    featureref_header_t* href = NULL;
    size_t size = CHUNK_SIZE + sizeof(uint32_t) * num_labels;

    /* Check if we aren't writing anything at this moment. */
    if (writer->state != WSTATE_NONE) {
        return CRFSUITEERR_INTERNAL_LOGIC;
    }

    /* Allocate a feature reference array. */
    href = (featureref_header_t*)calloc(size, 1);
    if (href == NULL) {
        return CRFSUITEERR_OUTOFMEMORY;
    }

    /* Align the offset to a DWORD boundary. */
    offset = (uint32_t)ftell(fp);
    while (offset % 4 != 0) {
        uint8_t c = 0;
        fwrite(&c, sizeof(uint8_t), 1, fp);
        ++offset;
    }

    /* Store the current offset position to the file header. */
    writer->header.off_labelrefs = offset;
    fseek(fp, size, SEEK_CUR);

    /* Fill members in the feature reference header. */
    memcpy(href->chunk, CHUNK_LABELREF, 4);
    href->size = 0;
    href->num = num_labels;

    writer->href = href;
    writer->state = WSTATE_LABELREFS;
    return 0;
}

int tag_crf1dmw::crf1dmw_close_labelrefs()
{
    tag_crf1dmw* writer = this;
    uint32_t i;
    FILE *fp = writer->fp;
    featureref_header_t* href = writer->href;
    uint32_t begin = writer->header.off_labelrefs, end = 0;

    /* Make sure that we are writing label feature references. */
    if (writer->state != WSTATE_LABELREFS) {
        return CRFSUITEERR_INTERNAL_LOGIC;
    }

    /* Store the current offset position. */
    end = (uint32_t)ftell(fp);

    /* Compute the size of this chunk. */
    href->size = (end - begin);

    /* Write the chunk header and offset array. */
    fseek(fp, begin, SEEK_SET);
    write_uint8_array(fp, href->chunk, 4);
    write_uint32(fp, href->size);
    write_uint32(fp, href->num);
    for (i = 0;i < href->num;++i) {
        write_uint32(fp, href->offsets[i]);
    }

    /* Move the file pointer to the tail. */
    fseek(fp, end, SEEK_SET);

    /* Uninitialize. */
    free(href);
    writer->href = NULL;
    writer->state = WSTATE_NONE;
    return 0;
}

int tag_crf1dmw::crf1dmw_put_labelref(int lid, const feature_refs_t* ref, int *map)
{
    tag_crf1dmw* writer = this;
    int i, fid;
    uint32_t n = 0, offset = 0;
    FILE *fp = writer->fp;
    featureref_header_t* href = writer->href;

    /* Make sure that we are writing label feature references. */
    if (writer->state != WSTATE_LABELREFS) {
        return CRFSUITEERR_INTERNAL_LOGIC;
    }

    /* Store the current offset to the offset array. */
    href->offsets[lid] = ftell(fp);

    /* Count the number of references to active features. */
    for (i = 0;i < ref->num_features;++i) {
        if (0 <= map[ref->fids[i]]) ++n;
    }

    /* Write the feature reference. */
    write_uint32(fp, (uint32_t)n);
    for (i = 0;i < ref->num_features;++i) {
        fid = map[ref->fids[i]];
        if (0 <= fid) write_uint32(fp, (uint32_t)fid);
    }

    return 0;
}

int tag_crf1dmw::crf1dmw_open_attrrefs(int num_attrs)
{
    tag_crf1dmw* writer = this;
    uint32_t offset;
    FILE *fp = writer->fp;
    featureref_header_t* href = NULL;
    size_t size = CHUNK_SIZE + sizeof(uint32_t) * num_attrs;

    /* Check if we aren't writing anything at this moment. */
    if (writer->state != WSTATE_NONE) {
        return CRFSUITEERR_INTERNAL_LOGIC;
    }

    /* Allocate a feature reference array. */
    href = (featureref_header_t*)calloc(size, 1);
    if (href == NULL) {
        return CRFSUITEERR_OUTOFMEMORY;
    }

    /* Align the offset to a DWORD boundary. */
    offset = (uint32_t)ftell(fp);
    while (offset % 4 != 0) {
        uint8_t c = 0;
        fwrite(&c, sizeof(uint8_t), 1, fp);
        ++offset;
    }

    /* Store the current offset position to the file header. */
    writer->header.off_attrrefs = offset;
    fseek(fp, size, SEEK_CUR);

    /* Fill members in the feature reference header. */
    memcpy(href->chunk, CHUNK_ATTRREF, 4);
    href->size = 0;
    href->num = num_attrs;

    writer->href = href;
    writer->state = WSTATE_ATTRREFS;
    return 0;
}

int tag_crf1dmw::crf1dmw_close_attrrefs()
{
    uint32_t i;
    FILE *fp = this->fp;
    featureref_header_t* href = this->href;
    uint32_t begin = this->header.off_attrrefs, end = 0;

    /* Make sure that we are writing attribute feature references. */
    if (this->state != WSTATE_ATTRREFS) {
        return CRFSUITEERR_INTERNAL_LOGIC;
    }

    /* Store the current offset position. */
    end = (uint32_t)ftell(fp);

    /* Compute the size of this chunk. */
    href->size = (end - begin);

    /* Write the chunk header and offset array. */
    fseek(fp, begin, SEEK_SET);
    write_uint8_array(fp, href->chunk, 4);
    write_uint32(fp, href->size);
    write_uint32(fp, href->num);
    for (i = 0;i < href->num;++i) {
        write_uint32(fp, href->offsets[i]);
    }

    /* Move the file pointer to the tail. */
    fseek(fp, end, SEEK_SET);

    /* Uninitialize. */
    free(href);
    this->href = NULL;
    this->state = WSTATE_NONE;
    return 0;
}

int tag_crf1dmw::crf1dmw_put_attrref(int aid, const feature_refs_t* ref, int *map)
{
    int i, fid;
    uint32_t n = 0, offset = 0;
    FILE *fp = this->fp;
    featureref_header_t* href = this->href;

    /* Make sure that we are writing attribute feature references. */
    if (this->state != WSTATE_ATTRREFS) {
        return CRFSUITEERR_INTERNAL_LOGIC;
    }

    /* Store the current offset to the offset array. */
    href->offsets[aid] = ftell(fp);

    /* Count the number of references to active features. */
    for (i = 0;i < ref->num_features;++i) {
        if (0 <= map[ref->fids[i]]) ++n;
    }

    /* Write the feature reference. */
    write_uint32(fp, (uint32_t)n);
    for (i = 0;i < ref->num_features;++i) {
        fid = map[ref->fids[i]];
        if (0 <= fid) write_uint32(fp, (uint32_t)fid);
    }

    return 0;
}

int tag_crf1dmw::crf1dmw_open_features()
{
    FILE *fp = this->fp;
    feature_header_t* hfeat = NULL;

    /* Check if we aren't writing anything at this moment. */
    if (this->state != WSTATE_NONE) {
        return CRFSUITEERR_INTERNAL_LOGIC;
    }

    /* Allocate a feature chunk header. */
    hfeat = (feature_header_t*)calloc(sizeof(feature_header_t), 1);
    if (hfeat == NULL) {
        return CRFSUITEERR_OUTOFMEMORY;
    }

    this->header.off_features = (uint32_t)ftell(fp);
    fseek(fp, CHUNK_SIZE, SEEK_CUR);

    memcpy(hfeat->chunk, CHUNK_FEATURE, 4);
    this->hfeat = hfeat;

    this->state = WSTATE_FEATURES;
    return 0;
}

int tag_crf1dmw::crf1dmw_close_features()
{
    FILE *fp = this->fp;
    feature_header_t* hfeat = this->hfeat;
    uint32_t begin = this->header.off_features, end = 0;

    /* Make sure that we are writing attribute feature references. */
    if (this->state != WSTATE_FEATURES) {
        return CRFSUITEERR_INTERNAL_LOGIC;
    }

    /* Store the current offset position. */
    end = (uint32_t)ftell(fp);

    /* Compute the size of this chunk. */
    hfeat->size = (end - begin);

    /* Write the chunk header and offset array. */
    fseek(fp, begin, SEEK_SET);
    write_uint8_array(fp, hfeat->chunk, 4);
    write_uint32(fp, hfeat->size);
    write_uint32(fp, hfeat->num);

    /* Move the file pointer to the tail. */
    fseek(fp, end, SEEK_SET);

    /* Uninitialize. */
    free(hfeat);
    this->hfeat = NULL;
    this->state = WSTATE_NONE;
    return 0;
}

int tag_crf1dmw::crf1dmw_put_feature(int fid, const crf1dm_feature_t* f)
{
    FILE *fp = this->fp;
    feature_header_t* hfeat = this->hfeat;

    /* Make sure that we are writing attribute feature references. */
    if (this->state != WSTATE_FEATURES) {
        return CRFSUITEERR_INTERNAL_LOGIC;
    }

    /* We must put features #0, #1, ..., #(K-1) in this order. */
    if (fid != hfeat->num) {
        return CRFSUITEERR_INTERNAL_LOGIC;
    }

    write_uint32(fp, f->type);
    write_uint32(fp, f->src);
    write_uint32(fp, f->dst);
    write_float(fp, f->weight);
    ++hfeat->num;
    return 0;
}

tag_crf1dm::tag_crf1dm(const void* buffer, size_t size)
{
    this->buffer = (const uint8_t*)buffer;

    if (size <= sizeof(header_t)) {
        printf("EEEE\n");
    }
    
    header_t *header = (header_t*)calloc(1, sizeof(header_t));
    if (header == NULL) {
        printf("EEEE\n");
    }

    /* Read the file header. */
    const uint8_t* p = this->buffer;
    p += read_uint8_array(p, header->magic, sizeof(header->magic));
    p += read_uint32(p, &header->size);
    p += read_uint8_array(p, header->type, sizeof(header->type));
    p += read_uint32(p, &header->version);
    p += read_uint32(p, &header->num_features);
    p += read_uint32(p, &header->num_labels);
    p += read_uint32(p, &header->num_attrs);
    p += read_uint32(p, &header->off_features);
    p += read_uint32(p, &header->off_labels);
    p += read_uint32(p, &header->off_attrs);
    p += read_uint32(p, &header->off_labelrefs);
    p += read_uint32(p, &header->off_attrrefs);
    this->header = header;

    this->labels = cqdb_reader(
        this->buffer + header->off_labels,
        size - header->off_labels
        );

    this->attrs = cqdb_reader(
        this->buffer + header->off_attrs,
        size - header->off_attrs
        );

    for (int i = 0; i < header->num_labels; ++i) {
        p = this->buffer;
        p += this->header->off_labelrefs;
        p += CHUNK_SIZE;
        p += sizeof(uint32_t) * i;
        uint32_t offset;
        uint32_t num_features;
        read_uint32(p, &offset);

        
        p = this->buffer + offset;
        p += read_uint32(p, &num_features);
        feature_refs_t ref;
        ref.num_features = num_features;
        uint32_t fid;
        for (int j = 0; j < ref.num_features; ++j) {
            read_uint32(p + sizeof(uint32_t)*j, &fid);
            ref.fids.push_back(fid);
        }
        this->label_refs.push_back(ref);
    }

    for (int i = 0; i < header->num_attrs; ++i) {
        p = this->buffer;
        p += this->header->off_attrrefs;
        p += CHUNK_SIZE;
        p += sizeof(uint32_t) * i;
        uint32_t offset;
        uint32_t num_features;
        read_uint32(p, &offset);

        p = this->buffer + offset;
        p += read_uint32(p, &num_features);
        feature_refs_t ref;
        ref.num_features = num_features;
        uint32_t fid;
        for (int j = 0; j < num_features; ++j) {
            read_uint32(p + sizeof(uint32_t)*j, &fid);
            ref.fids.push_back(fid);
        }
        this->attr_refs.push_back(ref);
    }
}

tag_crf1dm::tag_crf1dm(const char *filename)
{
    FILE *fp = NULL;
    uint32_t size = 0;
    uint8_t* buffer_orig = NULL;
    uint8_t* buffer = NULL;

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        throw std::runtime_error("fopen");
    }

    fseek(fp, 0, SEEK_END);
    size = (uint32_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buffer = buffer_orig = (uint8_t*)malloc(size + 16);
    if (buffer_orig == NULL) {
        throw std::runtime_error("malloc");
    }

    /* Align the buffer to 16 bytes. */
    while ((uintptr_t)buffer % 16 != 0) {
        ++buffer;
    }

    if (fread(buffer, 1, size, fp) != size) {
        throw std::runtime_error("fread");
    }
    fclose(fp);

    *this = tag_crf1dm(buffer, size);
}

tag_crf1dm::~tag_crf1dm()
{
    // if (this->labels != NULL) {
    //     cqdb_delete(this->labels);
    // }
    // if (this->attrs != NULL) {
    //     cqdb_delete(this->attrs);
    // }
    // if (this->header != NULL) {
    //     free(this->header);
    //     this->header = NULL;
    // }
    // if (this->buffer_orig != NULL) {
    //     free(this->buffer_orig);
    //     this->buffer_orig = NULL;
    // }
    // this->buffer = NULL;
}

int tag_crf1dm::crf1dm_get_num_attrs() { return this->header->num_attrs; }

int tag_crf1dm::crf1dm_get_num_labels() { return this->header->num_labels; }

const char *tag_crf1dm::crf1dm_to_label(int lid)
{
    if (this->labels != NULL) {
        return cqdb_to_string(this->labels, lid);
    } else {
        return NULL;
    }
}

int tag_crf1dm::crf1dm_to_lid(const char *value)
{
    if (this->labels != NULL) {
        return cqdb_to_id(this->labels, value);
    } else {
        return -1;
    }
}

int tag_crf1dm::crf1dm_to_aid(const char *value)
{
    if (this->attrs != NULL) {
        return cqdb_to_id(this->attrs, value);
    } else {
        return -1;
    }
}

const char *tag_crf1dm::crf1dm_to_attr(int aid)
{
    if (this->attrs != NULL) {
        return cqdb_to_string(this->attrs, aid);
    } else {
        return NULL;
    }
}

int tag_crf1dm::crf1dm_get_labelref(int lid, feature_refs_t* ref)
{
    *ref = this->label_refs[lid];
    return 0;
}

int tag_crf1dm::crf1dm_get_attrref(int aid, feature_refs_t* ref)
{
    *ref = this->attr_refs[aid];
    return 0;
}

int tag_crf1dm::crf1dm_get_featureid(feature_refs_t* ref, int i) {
    return ref->fids[i];   
}

int tag_crf1dm::crf1dm_get_feature(int fid, crf1dm_feature_t* f)
{
    const uint8_t *p = NULL;
    uint32_t val = 0;
    uint32_t offset = this->header->off_features + CHUNK_SIZE;
    offset += FEATURE_SIZE * fid;
    p = this->buffer + offset;
    p += read_uint32(p, &val);
    f->type = val;
    p += read_uint32(p, &val);
    f->src = val;
    p += read_uint32(p, &val);
    f->dst = val;
    p += read_float(p, &f->weight);
    return 0;
}

crfsuite_tagger_t* tag_crf1dm::get_tagger()
{
    /* Construct a tagger based on the model. */
    return new crf1dt_t(this);
}

void tag_crf1dm::dump(FILE *fp)
{
    int j;
    uint32_t i;
    feature_refs_t refs;
    const header_t* hfile = this->header;

    /* Dump the file header. */
    fprintf(fp, "FILEHEADER = {\n");
    fprintf(fp, "  magic: %c%c%c%c\n",
        hfile->magic[0], hfile->magic[1], hfile->magic[2], hfile->magic[3]);
    fprintf(fp, "  size: %" PRIu32 "\n", hfile->size);
    fprintf(fp, "  type: %c%c%c%c\n",
        hfile->type[0], hfile->type[1], hfile->type[2], hfile->type[3]);
    fprintf(fp, "  version: %" PRIu32 "\n", hfile->version);
    fprintf(fp, "  num_features: %" PRIu32 "\n", hfile->num_features);
    fprintf(fp, "  num_labels: %" PRIu32 "\n", hfile->num_labels);
    fprintf(fp, "  num_attrs: %" PRIu32 "\n", hfile->num_attrs);
    fprintf(fp, "  off_features: 0x%" PRIX32 "\n", hfile->off_features);
    fprintf(fp, "  off_labels: 0x%" PRIX32 "\n", hfile->off_labels);
    fprintf(fp, "  off_attrs: 0x%" PRIX32 "\n", hfile->off_attrs);
    fprintf(fp, "  off_labelrefs: 0x%" PRIX32 "\n", hfile->off_labelrefs);
    fprintf(fp, "  off_attrrefs: 0x%" PRIX32 "\n", hfile->off_attrrefs);
    fprintf(fp, "}\n");
    fprintf(fp, "\n");

    /* Dump the labels. */
    fprintf(fp, "LABELS = {\n");
    for (i = 0;i < hfile->num_labels;++i) {
        const char *str = this->crf1dm_to_label(i);
#if 0
        int check = crf1dm_to_lid(crf1dm, str);
        if (i != check) {
            fprintf(fp, "WARNING: inconsistent label CQDB\n");
        }
#endif
        fprintf(fp, "  %5" PRIu32 ": %s\n", i, str);
    }
    fprintf(fp, "}\n");
    fprintf(fp, "\n");

    /* Dump the attributes. */
    fprintf(fp, "ATTRIBUTES = {\n");
    for (i = 0;i < hfile->num_attrs;++i) {
        const char *str = this->crf1dm_to_attr(i);
#if 0
        int check = crf1dm_to_aid(crf1dm, str);
        if (i != check) {
            fprintf(fp, "WARNING: inconsistent attribute CQDB\n");
        }
#endif
        fprintf(fp, "  %5" PRIu32 ": %s\n", i, str);
    }
    fprintf(fp, "}\n");
    fprintf(fp, "\n");

    /* Dump the transition features. */
    fprintf(fp, "TRANSITIONS = {\n");
    for (i = 0;i < hfile->num_labels;++i) {
        this->crf1dm_get_labelref(i, &refs);
        for (j = 0;j < refs.num_features;++j) {
            crf1dm_feature_t f;
            int fid = this->crf1dm_get_featureid(&refs, j);
            const char *from = NULL, *to = NULL;

            this->crf1dm_get_feature(fid, &f);
            from = this->crf1dm_to_label(f.src);
            to = this->crf1dm_to_label(f.dst);
            fprintf(fp, "  (%d) %s --> %s: %f\n", f.type, from, to, f.weight);
        }
    }
    fprintf(fp, "}\n");
    fprintf(fp, "\n");

    /* Dump the transition features. */
    fprintf(fp, "STATE_FEATURES = {\n");
    for (i = 0;i < hfile->num_attrs;++i) {
        this->crf1dm_get_attrref(i, &refs);
        for (j = 0;j < refs.num_features;++j) {
            crf1dm_feature_t f;
            int fid = this->crf1dm_get_featureid(&refs, j);
            const char *attr = NULL, *to = NULL;

            this->crf1dm_get_feature(fid, &f);
#if 0
            if (f.src != i) {
                fprintf(fp, "WARNING: an inconsistent attribute reference.\n");
            }
#endif
            attr = this->crf1dm_to_attr(f.src);
            to = this->crf1dm_to_label(f.dst);
            fprintf(fp, "  (%d) %s --> %s: %f\n", f.type, attr, to, f.weight);
        }
    }
    fprintf(fp, "}\n");
    fprintf(fp, "\n");
}
