#ifndef MODEL_H
#define MODEL_H

#include "base.h"
#include "data_io.h"

#include <vector>
#include <set>
#include <map>
#include <memory>

namespace db_compress {

/*
 * ProbDist Class is initialized with two ProbIntervals and some Probability
 * Distribution, it reads in bit string, reaches certain unit bin in the distribution
 * and emits the result bin and two reduced ProbIntervals.
 */
class ProbDist {
  public:
    virtual ~ProbDist() = 0;
    virtual bool End() = 0;
    virtual void FeedByte(char byte) = 0;
    virtual ProbInterval GetPIt() = 0;
    virtual ProbInterval GetPIb() = 0;
    // Caller takes ownership of AttrValue.
    virtual AttrValue* GetResult() = 0;
    // Initialize the ProbDist, can also be used to reset it.
    virtual void Init(const ProbInterval& PIt, const ProbInterval& PIb) = 0;
};

inline ProbDist::~ProbDist() {}

/*
 * The Model class represents the local conditional probability distribution. The
 * Model object can be used to generate ProbDist object which can be used to infer
 * the result attribute value based on bitstring (decompressing). It can also be used to
 * create ProbInterval object which can be used for compressing.
 */
class Model {
  public:
    static const char TABLE_CATEGORY = 0;
    static const char TABLE_LAPLACE = 1;
    static const char STRING_MODEL = 2;
    virtual ~Model() = 0;
    // The Model class owns the ProbDist object.
    virtual ProbDist* GetProbDist(const Tuple& tuple, const ProbInterval& prob_interval) = 0;
    // If the model may modify the attributes during compression (lossy compression), then
    // resultAttr will be set as the modified result AttrValue, otherwise it may remain 
    // unaffected. The results are appended to the end of prob_intervals vector.
    virtual void GetProbInterval(const Tuple& tuple, std::vector<ProbInterval>* prob_intervals,
                                         std::unique_ptr<AttrValue>* result_attr) = 0;
    virtual const std::vector<size_t>& GetPredictorList() const = 0;
    virtual size_t GetTargetVar() const = 0;
    // Get an estimation of model cost, which is used in model selection process.
    virtual int GetModelCost() const = 0;

    // Learning
    virtual void FeedTuple(const Tuple& tuple) { }
    virtual void EndOfData() { }

    // Model Description
    virtual int GetModelDescriptionLength() const = 0;
    virtual void WriteModel(ByteWriter* byte_writer, size_t block_index) const = 0;
};

inline Model::~Model() {}

struct CompressionConfig {
    std::vector<double> allowed_err;
    // sort_by_attr = -1 means no specific sorting required
    int sort_by_attr;
};

Model* GetModelFromDescription(ByteReader* byte_reader, const Schema& schema, size_t index);

/*
 * The ModelLearner class learns all the models simultaneously in an online fashion.
 */
class ModelLearner {
  private:
    Schema schema_;
    CompressionConfig config_;
    int stage_;
    std::vector<size_t> ordered_attr_list_;
    std::vector< std::unique_ptr<Model> > active_model_list_;
    std::vector< std::unique_ptr<Model> > model_list_;
    std::vector< std::unique_ptr<Model> > selected_model_;
    std::vector< std::vector<size_t> > model_predictor_list_;
    std::map< std::pair<std::set<size_t>, size_t>, int> stored_model_cost_;
    std::set<size_t> trained_attr_list_;
    
    void InitModelList();
    void ExpandModelList();
    void StoreModelCost(const Model& model);
    // Get the model cost based on predictors and target variable.
    // If not known, return -1
    int GetModelCost(const Model& model) const;
  public:
    ModelLearner(const Schema& schema, const CompressionConfig& config);
    // These functions are used to learn the Model objects.
    void FeedTuple(const Tuple& tuple);
    bool RequireFullPass() const;
    bool RequireMoreIterations() const;
    void EndOfData();
    // This function gets the Model object for any particular attribute. Caller takes
    // ownership of the Model object. 
    Model* GetModel(size_t attr_index);
    // This function gets the order of attributes during the encoding/decoding phase
    const std::vector<size_t>& GetOrderOfAttributes() const;
};

} // namespace db_compress

#endif
