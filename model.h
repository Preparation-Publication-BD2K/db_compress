#ifndef MODEL_H
#define MODEL_H

#include "compression.h"

namespace db_compress {

/*
 * Structure used to represent any probability interval between [0, 1].
 */
struct ProbInterval {
};

/*
 * ProbDist Class is initialized with certain ProbInterval and some Probability
 * Distribution, it reads in bit string, reaches certain unit bin in the distribution
 * and emits the result bin, the result ProbInterval, and the remain unused bits.
 */
virtual class ProbDist {
  public:
    virtual bool End() = 0;
    // Will only use the last 8 bits.
    virtual void FeedByte(int byte) = 0;
    // The most significant bit is only used to indicate the end. Thus, 11001 represents
    // four unused bits 1001.
    virtual int GetUnusedBits() = 0;
    virtual const ProbInterval& GetRemainProbInterval() = 0;
    virtual AttrValue* GetResult() = 0;
    // Reset the ProbDist to its initial state, i.e., no bits has been read in.
    virtual void Reset() = 0;
};

/*
 * The Model class is used to generate ProbDist class.
 */
virtual class Model {
  public:
    // The Model class owns the ProbDist class
    virtual ProbDist* GetProbDist(const Tuple* tuple, const ProbInterval& prob_interval) = 0;
};

/*
 * This function gets the Model object from specific Schema and one particular attribute inside
 * the schema. 
 */
Model* GetModel(const Schema* schema, int attr_index);

/*
 * This function constructs all Model objects from the Schema object.
 */
void InitModel(const Schema* schema);

/*
 * These functions are used to learn the Model objects.
 */
void FeedTuple(const Tuple* tuple);
bool RequireModeIterations();
void EndOfData();

} // namespace db_compress

#endif
