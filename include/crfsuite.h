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

#ifndef    __CRFSUITE_H__
#define    __CRFSUITE_H__

#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <vector>

/** 
 * \addtogroup crfsuite_api CRFSuite C API
 * @{
 *
 *  The CRFSuite C API provides a low-level library for manupulating
 *  CRFSuite in C language.
 */

/** 
 * \addtogroup crfsuite_misc Miscellaneous definitions and functions
 * @{
 */

/** Version number of CRFSuite library. */
#define CRFSUITE_VERSION    "0.12.2"

/** Copyright string of CRFSuite library. */
#define CRFSUITE_COPYRIGHT  "Copyright (c) 2007-2013 Naoaki Okazaki"

/** Type of a float value. */
typedef double floatval_t;

/** Maximum value of a float value. */
#define    FLOAT_MAX    DBL_MAX

/**
 * Status codes.
 */
enum {
    /** Success. */
    CRFSUITE_SUCCESS = 0,
    /** Unknown error occurred. */
    CRFSUITEERR_UNKNOWN = INT_MIN,
    /** Insufficient memory. */
    CRFSUITEERR_OUTOFMEMORY,
    /** Unsupported operation. */
    CRFSUITEERR_NOTSUPPORTED,
    /** Incompatible data. */
    CRFSUITEERR_INCOMPATIBLE,
    /** Internal error. */
    CRFSUITEERR_INTERNAL_LOGIC,
    /** Overflow. */
    CRFSUITEERR_OVERFLOW,
    /** Not implemented. */
    CRFSUITEERR_NOTIMPLEMENTED,
};

/**@}*/



/**
 * \addtogroup crfsuite_object Object interfaces and utilities.
 * @{
 */

struct tag_crfsuite_model;
/** CRFSuite model interface. */
typedef struct tag_crfsuite_model crfsuite_model_t;

struct tag_crfsuite_trainer;
/** CRFSuite trainer interface. */
typedef struct tag_crfsuite_trainer crfsuite_trainer_t;

struct tag_crfsuite_tagger;
/** CRFSuite tagger interface. */
typedef struct tag_crfsuite_tagger crfsuite_tagger_t;

struct tag_crfsuite_dictionary;
/** CRFSuite dictionary interface. */
typedef struct tag_crfsuite_dictionary crfsuite_dictionary_t;

struct tag_crfsuite_params;
/** CRFSuite parameter interface. */
typedef struct tag_crfsuite_params crfsuite_params_t;

/**@}*/



/**
 * \addtogroup crfsuite_data Dataset (attribute, item, instance, dataset)
 * @{
 */

/**
 * An attribute.
 *  An attribute consists of an attribute id with its value.
 */
struct crfsuite_attribute_t {
    int         aid;                /**< Attribute id. */
    floatval_t  value;              /**< Value of the attribute. */
public:
    crfsuite_attribute_t(int aid, floatval_t value) : aid(aid), value(value) {}
    crfsuite_attribute_t(int aid) : aid(aid), value(1.0) {}
    crfsuite_attribute_t() : aid(0), value(1.0) {}
};

/**
 * An item.
 *  An item consists of an array of attributes.
 */
struct crfsuite_item_t {
    /** Number of contents associated with the item. */
    size_t             num_contents() const { return this->contents.size(); }
    /** Maximum number of contents (internal use). */
    size_t             cap_contents() const { return this->contents.capacity(); }
    /** Array of the attributes. */
    std::vector<crfsuite_attribute_t>    contents;
public:
    void append(const crfsuite_attribute_t& cont) { this->contents.push_back(cont); }
};

/**
 * An instance (sequence of items and labels).
 *  An instance consists of a sequence of items and labels.
 */
struct crfsuite_instance_t {
    /** Number of items/labels in the sequence. */
    size_t         num_items() const { return this->items.size(); }
    /** Maximum number of items/labels (internal use). */
    size_t         cap_items() const { return this->items.capacity(); }
    /** Array of the item sequence. */
    std::vector<crfsuite_item_t>  items;
    /** Array of the label sequence. */
    std::vector<int>         labels;
    /** Instance weight. */
    floatval_t  weight;
    /** Group ID of the instance. */
	int         group;
public:
    crfsuite_instance_t() : weight(1.0), group(0) {}

    void append(const crfsuite_item_t& item, int label)
    {
        this->items.push_back(item);
        this->labels.push_back(label);
    }

    void clear() {
        this->items.clear();
        this->labels.clear();
    }

    bool empty() const { return (this->num_items() == 0); }
};

/**
 * A data set.
 *  A data set consists of an array of instances and dictionary objects
 *  for attributes and labels.
 */
struct crfsuite_data_t{
    /** Number of instances. */
    size_t                 num_instances() const { return this->instances.size(); }
    /** Maximum number of instances (internal use). */
    size_t                 cap_instances() const { return this->instances.capacity(); }
    /** Array of instances. */
    std::vector<crfsuite_instance_t>     instances;

    /** Dictionary object for attributes. */
    crfsuite_dictionary_t    *attrs;
    /** Dictionary object for labels. */
    crfsuite_dictionary_t    *labels;
public:

    void append(const crfsuite_instance_t& inst)
    {
        if (0 < inst.num_items()) {
            this->instances.push_back(inst);
        }
    }

    size_t maxlength() const
    {
        int i, T = 0;
        for (i = 0;i < this->num_instances();++i) {
            if (T < this->instances[i].num_items()) {
                T = this->instances[i].num_items();
            }
        }
        return T;
    }

    size_t totalitems() const
    {
        int i, n = 0;
        for (i = 0;i < this->num_instances();++i) {
            n += this->instances[i].num_items();
        }
        return n;
    }
} ;

/**@}*/



/**
 * \addtogroup crfsuite_evaluation Evaluation utility
 * @{
 */

/**
 * Label-wise performance values.
 */
 struct crfsuite_label_evaluation_t {
    /** Number of correct predictions. */
    int         num_correct;
    /** Number of occurrences of the label in the gold-standard data. */
    int         num_observation;
    /** Number of predictions. */
    int         num_model;
    /** Precision. */
    floatval_t  precision;
    /** Recall. */
    floatval_t  recall;
    /** F1 score. */
    floatval_t  fmeasure;
} ;

/**
 * An overall performance values.
 */
 struct crfsuite_evaluation_t{
    /** Number of labels. */
    int         num_labels;
    /** Array of label-wise evaluations. */
    crfsuite_label_evaluation_t* tbl;

    /** Number of correctly predicted items. */
    int         item_total_correct;
    /** Total number of items. */
    int         item_total_num;
    /** Total number of occurrences of labels in the gold-standard data. */
    int         item_total_observation;
    /** Total number of predictions. */
    int         item_total_model;
    /** Item-level accuracy. */
    floatval_t  item_accuracy;

    /** Number of correctly predicted instances. */
    int         inst_total_correct;
    /** Total number of instances. */
    int         inst_total_num;
    /** Instance-level accuracy. */
    floatval_t  inst_accuracy;

    /** Macro-averaged precision. */
    floatval_t  macro_precision;
    /** Macro-averaged recall. */
    floatval_t  macro_recall;
    /** Macro-averaged F1 score. */
    floatval_t  macro_fmeasure;
} ;

/**@}*/


/**
 * \addtogroup crfsuite_object
 * @{
 */

/**
 * Type of callback function for logging.
 *  @param  user        Pointer to the user-defined data.
 *  @param  format      Format string (compatible with prinf()).
 *  @param  args        Optional arguments for the format string.
 *  @return int         \c 0 to continue; non-zero to cancel the training.
 */
typedef int (*crfsuite_logging_callback)(void *user, const char *format, va_list args);


/**
 * CRFSuite model interface.
 */
struct tag_crfsuite_model {
    /**
     * Obtain the pointer to crfsuite_tagger_t interface.
     *  @param  model       The pointer to this model instance.
     *  @param  ptr_tagger  The pointer that receives a crfsuite_tagger_t
     *                      pointer.
     *  @return int         The status code.
     */
    virtual crfsuite_tagger_t* get_tagger() = 0;

    /**
     * Obtain the pointer to crfsuite_dictionary_t interface for labels.
     *  @param  model       The pointer to this model instance.
     *  @param  ptr_labels  The pointer that receives a crfsuite_dictionary_t
     *                      pointer.
     *  @return int         The status code.
     */
    virtual crfsuite_dictionary_t* get_labels() = 0;

    /**
     * Obtain the pointer to crfsuite_dictionary_t interface for attributes.
     *  @param  model       The pointer to this model instance.
     *  @param  ptr_attrs   The pointer that receives a crfsuite_dictionary_t
     *                      pointer.
     *  @return int         The status code.
     */
    virtual crfsuite_dictionary_t* get_attrs() = 0;

    /**
     * Print the model in human-readable format.
     *  @param  model       The pointer to this model instance.
     *  @param  fpo         The FILE* pointer.
     *  @return int         The status code.
     */
    virtual void dump(FILE *fpo) = 0;
};

int crfsuite_dictionary_create_instance(const char *interface, void **ptr);

/**
 * CRFSuite trainer interface.
 */
struct tag_crfsuite_trainer {
public:
    // tag_crfsuite_trainer(int ftype, int algorithm);
    // ~tag_crfsuite_trainer();
    /**
     * Obtain the pointer to crfsuite_params_t interface.
     *  @param  trainer     The pointer to this trainer instance.
     *  @return crfsuite_params_t*  The pointer to crfsuite_params_t.
     */
    virtual tag_crfsuite_params* params() = 0;

    /**
     * Set the callback function and user-defined data.
     *  @param  trainer     The pointer to this trainer instance.
     *  @param  user        The pointer to the user-defined data.
     *  @param  cbm         The pointer to the callback function.
     */
    virtual void set_message_callback(void *user, crfsuite_logging_callback cbm) = 0;

    /**
     * Start a training process.
     *  @param  trainer     The pointer to this trainer instance.
     *  @param  data        The poiinter to the data set.
     *  @param  filename    The filename to which the trainer stores the model.
     *                      If an empty string is specified, this function
     *                      does not sture the model to a file.
     *  @param  holdout     The holdout group.
     *  @return int         The status code.
     */
    virtual int train(const crfsuite_data_t *data, const char *filename, int holdout) = 0;
};

/**
 * CRFSuite tagger interface.
 */
struct tag_crfsuite_tagger {
    /**
     * Set an instance to the tagger.
     *  @param  tagger      The pointer to this tagger instance.
     *  @param  inst        The item sequence to be tagged.
     *  @return int         The status code.
     */
    virtual int set(crfsuite_instance_t *inst) = 0;

    /**
     * Obtain the number of items in the current instance.
     *  @param  tagger      The pointer to this tagger instance.
     *  @return int         The number of items of the instance set by
     *                      set() function.
     *  @return int         The status code.
     */
    virtual int length() const = 0;

    /**
     * Find the Viterbi label sequence.
     *  @param  tagger      The pointer to this tagger instance.
     *  @param  labels      The label array that receives the Viterbi label
     *                      sequence. The number of elements in the array must
     *                      be no smaller than the number of item.
     *  @param  ptr_score   The pointer to a float variable that receives the
     *                      score of the Viterbi label sequence.
     *  @return int         The status code.
     */
    virtual floatval_t viterbi(std::vector<int>& labels) = 0;

    


    /**
     * Compute the log of the partition factor (normalization constant).
     *  @param  tagger      The pointer to this tagger instance.
     *  @param  ptr_score   The pointer to a float variable that receives the
     *                      logarithm of the partition factor.
     *  @return int         The status code.
     */
    virtual floatval_t lognorm() = 0;

    /**
     * Compute the marginal probability of a label at a position.
     *  This function computes P(y_t = l | x), the probability when
     *  y_t is the label (l).
     *  @param  tagger      The pointer to this tagger instance.
     *  @param  l           The label.
     *  @param  t           The position.
     *  @param  ptr_prob    The pointer to a float variable that receives the
     *                      marginal probability.
     *  @return int         The status code.
     */
    virtual floatval_t marginal_point(int l, int t) = 0;

    /**
     * Compute the marginal probability of a partial label sequence.
     *  @param  tagger      The pointer to this tagger instance.
     *  @param  path        The partial label sequence.
     *  @param  begin       The start position of the partial label sequence.
     *  @param  end         The last+1 position of the partial label sequence.
     *  @param  ptr_prob    The pointer to a float variable that receives the
     *                      marginal probability.
     *  @return int         The status code.
     */
    virtual floatval_t marginal_path(const int *path, int begin, int end) = 0;
    /**
     * Compute the score of a label sequence.
     *  @param  tagger      The pointer to this tagger instance.
     *  @param  path        The label sequence.
     *  @param  ptr_score   The pointer to a float variable that receives the
     *                      score of the label sequence.
     *  @return int         The status code.
     */
    virtual floatval_t score(std::vector<int>& path) = 0;
};

/**
 * CRFSuite dictionary interface.
 */
struct tag_crfsuite_dictionary {
    /**
     * Assign and obtain the integer ID for the string.
     *  @param  dic         The pointer to this dictionary instance.
     *  @param  str         The string.
     *  @return int         The ID associated with the string if any,
     *                      the new ID otherwise.
     */
    virtual int get(const char *str) = 0;

    /**
     * Obtain the integer ID for the string.
     *  @param  dic         The pointer to this dictionary instance.
     *  @param  str         The string.
     *  @return int         The ID associated with the string if any,
     *                      \c -1 otherwise.
     */
    virtual int to_id(const char *str) = 0;

    /**
     * Obtain the string for the ID.
     *  @param  dic         The pointer to this dictionary instance.
     *  @param  id          the string ID.
     *  @param  pstr        \c *pstr points to the string associated with
     *                      the ID if any, \c NULL otherwise.
     *  @return int         \c 0 if the string ID is associated with a string,
     *                      \c 1 otherwise.
     */
    virtual int to_string(int id, char const **pstr) = 0;

    /**
     * Obtain the number of strings in the dictionary.
     *  @param  dic         The pointer to this dictionary instance.
     *  @return int         The number of strings stored in the dictionary.
     */
    virtual int num() = 0;

    /**
     * Free the memory block allocated by to_string() function.
     *  @param  dic         The pointer to this dictionary instance.
     *  @param  str         The pointer to the string whose memory block is
     *                      freed.
     */
    virtual void free(const char *str) = 0;
};

/**
 * CRFSuite parameter interface.
 */
struct tag_crfsuite_params {
    /**
     * Pointer to the instance data (internal use only).
     */
    void *internal;

    /**
     * Reference counter (internal use only).
     */
    int nref;

    /**
     * Increment the reference counter.
     *  @param  params      The pointer to this parameter instance.
     *  @return int         The reference count after this increment.
     */
    int (*addref)(crfsuite_params_t* params);

    /**
     * Decrement the reference counter.
     *  @param  params      The pointer to this parameter instance.
     *  @return int         The reference count after this operation.
     */
    int (*release)(crfsuite_params_t* params);

    /**
     * Obtain the number of available parameters.
     *  @param  params      The pointer to this parameter instance.
     *  @return int         The number of parameters maintained by this object.
     */
    int (*num)(crfsuite_params_t* params);

    /**
     * Obtain the name of a parameter.
     *  @param  params      The pointer to this parameter instance.
     *  @param  i           The parameter index.
     *  @param  ptr_name    *ptr_name points to the parameter name.
     *  @return int         \c 0 always.
     */
    int (*name)(crfsuite_params_t* params, int i, char **ptr_name);

    /**
     * Set a parameter value.
     *  @param  params      The pointer to this parameter instance.
     *  @param  name        The parameter name.
     *  @param  value       The parameter value in string format.
     *  @return int         \c 0 if the parameter is found, \c -1 otherwise.
     */
    int (*set)(crfsuite_params_t* params, const char *name, const char *value);

    /**
     * Get a parameter value.
     *  @param  params      The pointer to this parameter instance.
     *  @param  name        The parameter name.
     *  @param  ptr_value   *ptr_value presents the parameter value in string
     *                      format.
     *  @return int         \c 0 if the parameter is found, \c -1 otherwise.
     */
    int (*get)(crfsuite_params_t* params, const char *name, char **ptr_value);

    /**
     * Set an integer value of a parameter.
     *  @param  params      The pointer to this parameter instance.
     *  @param  name        The parameter name.
     *  @param  value       The parameter value.
     *  @return int         \c 0 if the parameter value is set successfully,
     *                      \c -1 otherwise (unknown parameter or incompatible
     *                      type).
     */
    int (*set_int)(crfsuite_params_t* params, const char *name, int value);

    /**
     * Set a float value of a parameter.
     *  @param  params      The pointer to this parameter instance.
     *  @param  name        The parameter name.
     *  @param  value       The parameter value.
     *  @return int         \c 0 if the parameter value is set successfully,
     *                      \c -1 otherwise (unknown parameter or incompatible
     *                      type).
     */
    int (*set_float)(crfsuite_params_t* params, const char *name, floatval_t value);

    /**
     * Set a string value of a parameter.
     *  @param  params      The pointer to this parameter instance.
     *  @param  name        The parameter name.
     *  @param  value       The parameter value.
     *  @return int         \c 0 if the parameter value is set successfully,
     *                      \c -1 otherwise (unknown parameter or incompatible
     *                      type).
     */
    int (*set_string)(crfsuite_params_t* params, const char *name, const char *value);

    /**
     * Get an integer value of a parameter.
     *  @param  params      The pointer to this parameter instance.
     *  @param  name        The parameter name.
     *  @param  ptr_value   The pointer to a variable that receives the
     *                      integer value.
     *  @return int         \c 0 if the parameter value is obtained
     *                      successfully, \c -1 otherwise (unknown parameter
     *                      or incompatible type).
     */
    int (*get_int)(crfsuite_params_t* params, const char *name, int *ptr_value);

    /**
     * Get a float value of a parameter.
     *  @param  params      The pointer to this parameter instance.
     *  @param  name        The parameter name.
     *  @param  ptr_value   The pointer to a variable that receives the
     *                      float value.
     *  @return int         \c 0 if the parameter value is obtained
     *                      successfully, \c -1 otherwise (unknown parameter
     *                      or incompatible type).
     */
    int (*get_float)(crfsuite_params_t* params, const char *name, floatval_t *ptr_value);

    /**
     * Get a string value of a parameter.
     *  @param  params      The pointer to this parameter instance.
     *  @param  name        The parameter name.
     *  @param  ptr_value   *ptr_value presents the parameter value.
     *  @return int         \c 0 if the parameter value is obtained
     *                      successfully, \c -1 otherwise (unknown parameter
     *                      or incompatible type).
     */
    int (*get_string)(crfsuite_params_t* params, const char *name, char **ptr_value);

    /**
     * Get the help message of a parameter.
     *  @param  params      The pointer to this parameter instance.
     *  @param  name        The parameter name.
     *  @param  ptr_type    The pointer to \c char* to which this function
     *                      store the type of the parameter.
     *  @param  ptr_help    The pointer to \c char* to which this function
     *                      store the help message of the parameter.
     *  @return int         \c 0 if the parameter is found, \c -1 otherwise.
     */
    int (*help)(crfsuite_params_t* params, const char *name, char **ptr_type, char **ptr_help);

    /**
     * Free the memory block of a string allocated by this object.
     *  @param  params      The pointer to this parameter instance.
     *  @param  str         The pointer to the string.
     */
    void (*free)(crfsuite_params_t* params, const char *str);
};

/**@}*/



/**
 * \addtogroup crfsuite_object
 * @{
 */

/**
 * Create an instance of an object by an interface identifier.
 *  @param  iid         The interface identifier.
 *  @param  ptr         The pointer to \c void* that points to the
 *                      instance of the object if successful,
 *                      *ptr points to \c NULL otherwise.
 *  @return int         \c 1 if this function creates an object successfully,
 *                      \c 0 otherwise. Note that this is inconsistent with the
 *                      other CRFsuite API calls.
 */
int crfsuite_create_instance(const char *iid, void **ptr);

/**
 * Create an instance of a model object from a model file.
 *  @param  filename    The filename of the model.
 *  @param  ptr         The pointer to \c void* that points to the
 *                      instance of the model object if successful,
 *                      *ptr points to \c NULL otherwise.
 *  @return int         \c 0 if this function creates an object successfully,
 *                      \c 1 otherwise.
 */
int crfsuite_create_instance_from_file(const char *filename, void **ptr);

/**
 * Create an instance of a model object from a model in memory.
 *  @param  data        A pointer to the model data.
 *                      Must be 16-byte aligned.
 *  @param  size        A size (in bytes) of the model data.
 *  @param  ptr         The pointer to \c void* that points to the
 *                      instance of the model object if successful,
 *                      *ptr points to \c NULL otherwise.
 *  @return int         \c 0 if this function creates an object successfully,
 *                      \c 1 otherwise
 */
int crfsuite_create_instance_from_memory(const void *data, size_t size, void **ptr);

/**
 * Create instances of tagging object from a model file.
 *  @param  filename    The filename of the model.
 *  @param  ptr_tagger  The pointer to \c void* that points to the
 *                      instance of the tagger object if successful,
 *                      *ptr points to \c NULL otherwise.
 *  @param  ptr_attrs   The pointer to \c void* that points to the
 *                      instance of the dictionary object for attributes
 *                      if successful, *ptr points to \c NULL otherwise.
 *  @param  ptr_labels  The pointer to \c void* that points to the
 *                      instance of the dictionary object for labels
 *                      if successful, *ptr points to \c NULL otherwise.
 *  @return int         \c 0 if this function creates an object successfully,
 *                      \c 1 otherwise.
 */
int crfsuite_create_tagger(
    const char *filename,
    crfsuite_tagger_t** ptr_tagger,
    crfsuite_dictionary_t** ptr_attrs,
    crfsuite_dictionary_t** ptr_labels
    );

/**@}*/



/**
 * \addtogroup crfsuite_data
 * @{
 */

/**@}*/

/**
 * \addtogroup crfsuite_evaluation
 */
/**@{*/

/**
 * Initialize an evaluation structure.
 *  @param  eval        The pointer to crfsuite_evaluation_t.
 *  @param  n           The number of labels in the dataset.
 */
void crfsuite_evaluation_init(crfsuite_evaluation_t* eval, int n);

/**
 * Uninitialize an evaluation structure.
 *  @param  eval        The pointer to crfsuite_evaluation_t.
 */
void crfsuite_evaluation_finish(crfsuite_evaluation_t* eval);

/**
 * Reset an evaluation structure.
 *  @param  eval        The pointer to crfsuite_evaluation_t.
 */
void crfsuite_evaluation_clear(crfsuite_evaluation_t* eval);

/**
 * Accmulate the correctness of the predicted label sequence.
 *  @param  eval        The pointer to crfsuite_evaluation_t.
 *  @param  reference   The reference label sequence.
 *  @param  prediction  The predicted label sequence.
 *  @param  T           The length of the label sequence.
 *  @return int         \c 0 if succeeded, \c 1 otherwise.
 */
int crfsuite_evaluation_accmulate(crfsuite_evaluation_t* eval, const std::vector<int>& reference, const std::vector<int>& prediction, int T);

/**
 * Finalize the evaluation result.
 *  @param  eval        The pointer to crfsuite_evaluation_t.
 */
void crfsuite_evaluation_finalize(crfsuite_evaluation_t* eval);

/**
 * Print the evaluation result.
 *  @param  eval        The pointer to crfsuite_evaluation_t.
 *  @param  labels      The pointer to the label dictionary.
 *  @param  cbm         The callback function to receive the evaluation result.
 *  @param  user        The pointer to the user data that is forwarded to the
 *                      callback function.
 */
void crfsuite_evaluation_output(crfsuite_evaluation_t* eval, crfsuite_dictionary_t* labels, crfsuite_logging_callback cbm, void *user);

/**@}*/



/** 
 * \addtogroup crfsuite_misc Miscellaneous definitions and functions
 * @{
 */

/**
 * Increments the value of the integer variable as an atomic operation.
 *  @param  count       The pointer to the integer variable.
 *  @return             The value after this increment.
 */
int crfsuite_interlocked_increment(int *count);

/**
 * Decrements the value of the integer variable as an atomic operation.
 *  @param  count       The pointer to the integer variable.
 *  @return             The value after this decrement.
 */
int crfsuite_interlocked_decrement(int *count);

/**@}*/

/**@}*/

/**
@mainpage CRFsuite: a fast implementation of Conditional Random Fields (CRFs)

@section intro Introduction

This document describes information for using
<a href="http://www.chokkan.org/software/crfsuite">CRFsuite</a> from external
programs. CRFsuite provides two APIs:
- @link crfsuite_api C API @endlink: low-level and complete interface, which
  is used by the official frontend program.
- @link crfsuite_hpp_api C++/SWIG API @endlink: high-level and easy-to-use
  interface for a number of programming languages (e.g, C++ and Python),
  which is a wrapper for the C API.

*/

#endif/*__CRFSUITE_H__*/
