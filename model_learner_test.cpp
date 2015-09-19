#include "base.h"
#include "model.h"
#include "model_learner.h"

#include <cmath>
#include <vector>
#include <iostream>

namespace db_compress {

std::map<int, int> model_cost;
CompressionConfig config;
Schema schema;

class MockModel : public Model {
  public:
    MockModel(const std::vector<size_t>& pred, size_t target) : Model(pred, target) {}
    ProbTree* GetProbTree(const Tuple& tuple) { return NULL; }
    int GetModelDescriptionLength() const { return 0; }
    void WriteModel(ByteWriter* byte_writer, size_t block_index) const {}
    int GetModelCost() const {
        int index = 0;
        for (size_t i = 0; i < predictor_list_.size(); ++i)
            index = index * 10 + predictor_list_[i] + 1;
        index = index * 10 + target_var_ + 1;
        if (model_cost[index] == 0) return 1000000;
        return model_cost[index];
    }
};

class MockModelCreator : public ModelCreator {
  public:
    Model* ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index) { return NULL; }
    Model* CreateModel(const Schema& schema, const std::vector<size_t>& pred,
                       size_t index, double err) { return new MockModel(pred, index); }
};

void PrepareData() {
    model_cost[1] = 9;
    model_cost[2] = 8;
    model_cost[3] = 7;
    model_cost[21] = 4;
    model_cost[231] = 2;
    model_cost[12] = 12;
    model_cost[23] = 5;
    model_cost[32] = 5;
    std::vector<int> attr(3);
    attr[0] = attr[1] = attr[2] = 0;
    schema = Schema(attr);
    RegisterAttrModel(0, new MockModelCreator());
    config.allowed_err.resize(3);
}

void TestWithPrimaryAttr() {
    config.sort_by_attr = 0;
    ModelLearner learner(schema, config);
    while (1) {
        learner.EndOfData();
        if (!learner.RequireMoreIterations())
            break;
    }
    std::vector<size_t> attr_vec = learner.GetOrderOfAttributes();
    if (attr_vec[0] != 0 || attr_vec[1] != 2 || attr_vec[2] != 1)
        std::cerr << attr_vec[1] << "Model Learner w/ Primary Attr Unit Test Failed!\n";
    std::unique_ptr<Model> a(learner.GetModel(0));
    std::unique_ptr<Model> b(learner.GetModel(1));
    std::unique_ptr<Model> c(learner.GetModel(2));
    if (a->GetTargetVar() != 0 || a->GetPredictorList().size() != 0)
        std::cerr << "Model Learner w/ Primary Attr Unit Test Failed!\n";
    if (b->GetTargetVar() != 1 || b->GetPredictorList().size() != 1 ||
        b->GetPredictorList()[0] != 2)
        std::cerr << "Model Learner w/ Primary Attr Unit Test Failed!\n";
    if (c->GetTargetVar() != 2 || c->GetPredictorList().size() != 0)
        std::cerr << "Model Learner w/ Primary Attr Unit Test Failed!\n";
}

void TestWithoutPrimaryAttr() {
    config.sort_by_attr = -1;
    ModelLearner learner(schema, config);
    while (1) {
        learner.EndOfData();
        if (!learner.RequireMoreIterations())
            break;
    }
    std::vector<size_t> attr_vec = learner.GetOrderOfAttributes();
    if (attr_vec[0] != 2 || attr_vec[1] != 1 || attr_vec[2] != 0)
        std::cerr << "Model Learner w/o Primary Attr Unit Test Failed!\n";
    std::unique_ptr<Model> a(learner.GetModel(0));
    std::unique_ptr<Model> b(learner.GetModel(1));
    std::unique_ptr<Model> c(learner.GetModel(2));
    if (a->GetTargetVar() != 0 || a->GetPredictorList().size() != 2 ||
        a->GetPredictorList()[0] != 1 || a->GetPredictorList()[1] != 2)
        std::cerr << "Model Learner w/o Primary Attr Unit Test Failed!\n";
    if (b->GetTargetVar() != 1 || b->GetPredictorList().size() != 1 ||
        b->GetPredictorList()[0] != 2)
        std::cerr << "Model Learner w/o Primary Attr Unit Test Failed!\n";
    if (c->GetTargetVar() != 2 || c->GetPredictorList().size() != 0)
        std::cerr << "Model Learner w/o Primary Attr Unit Test Failed!\n";
}

void Test() {
    PrepareData();
    TestWithPrimaryAttr();
    TestWithoutPrimaryAttr();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
