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

inline int GetCost(const std::vector<size_t>& pred, int target) {
    int index = 0;
    for (size_t i = 0; i < pred.size(); ++i)
        index = index * 10 + pred[i] + 1;
    index = index * 10 + target + 1;
    if (model_cost[index] == 0) return 1000000;
    return model_cost[index];
}

class MockAttr : public AttrValue {
  private:
    int val_;
  public:
    MockAttr(int val) : val_(val) {}
    int Value() const { return val_; }
};

class MockTree : public SquID {
  private:
    bool first_step_;
    MockAttr attr_;
  public:
    MockTree() : first_step_(true), attr_(0) {}
    bool HasNextBranch() const { return first_step_; }
    void GenerateNextBranch() { first_step_ = false; }
    int GetNextBranch(const AttrValue* attr) const { return 0; }
    void ChooseNextBranch(int branch) {}
    const AttrValue* GetResultAttr() { return &attr_; }
};

class MockModel : public SquIDModel {
  private:
    MockTree tree_;
    int a_, b_, c_;
  public:
    MockModel(const std::vector<size_t>& pred, size_t target) : SquIDModel(pred, target) {}
    SquID* GetSquID(const Tuple& tuple) { return &tree_; }
    void FeedTuple(const Tuple& tuple) {
        a_ = static_cast<const MockAttr*>(tuple.attr[0])->Value();
        b_ = static_cast<const MockAttr*>(tuple.attr[1])->Value();
        c_ = static_cast<const MockAttr*>(tuple.attr[2])->Value();
    }
    int Check() const { return a_* 100 + b_ * 10 + c_; }
    int GetModelDescriptionLength() const { return 0; }
    void WriteModel(ByteWriter* byte_writer, size_t block_index) const {}
    int GetModelCost() const {
        return GetCost(predictor_list_, target_var_);
    }
};

class MockModelCreator : public ModelCreator {
  public:
    SquIDModel* ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index) 
        { return NULL; }
    SquIDModel* CreateModel(const Schema& schema, const std::vector<size_t>& pred,
                       size_t index, double err) {
        int cost = GetCost(pred, index);
        if (cost == 1000000)
            return NULL;
        return new MockModel(pred, index);
    }
};

inline int Check(SquIDModel* model) {
    return static_cast<MockModel*>(model)->Check();
}

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
    MockAttr attr(1);
    Tuple tuple(3); 
    tuple.attr[0] = tuple.attr[1] = tuple.attr[2] = &attr;
    while (1) {
        learner.FeedTuple(tuple);
        learner.EndOfData();
        if (!learner.RequireMoreIterations())
            break;
    }
    std::vector<size_t> attr_vec = learner.GetOrderOfAttributes();
    if (attr_vec[0] != 0 || attr_vec[1] != 2 || attr_vec[2] != 1)
        std::cerr << attr_vec[1] << "Model Learner w/ Primary Attr Unit Test Failed!\n";
    std::unique_ptr<SquIDModel> a(learner.GetModel(0));
    std::unique_ptr<SquIDModel> b(learner.GetModel(1));
    std::unique_ptr<SquIDModel> c(learner.GetModel(2));
    if (a->GetTargetVar() != 0 || a->GetPredictorList().size() != 0 || Check(a.get()) != 111)
        std::cerr << "Model Learner w/ Primary Attr Unit Test Failed!\n";
    if (b->GetTargetVar() != 1 || b->GetPredictorList().size() != 1 ||
        b->GetPredictorList()[0] != 2 || Check(b.get()) != 10)
        std::cerr << "Model Learner w/ Primary Attr Unit Test Failed!\n";
    if (c->GetTargetVar() != 2 || c->GetPredictorList().size() != 0 || Check(c.get()) != 111) 
        std::cerr << "Model Learner w/ Primary Attr Unit Test Failed!\n";
}

void TestWithoutPrimaryAttr() {
    config.sort_by_attr = -1;
    ModelLearner learner(schema, config);
    MockAttr attr(1);
    Tuple tuple(3); 
    tuple.attr[0] = tuple.attr[1] = tuple.attr[2] = &attr;
    while (1) {
        learner.FeedTuple(tuple);
        learner.EndOfData();
        if (!learner.RequireMoreIterations())
            break;
    }
    std::vector<size_t> attr_vec = learner.GetOrderOfAttributes();
    if (attr_vec[0] != 2 || attr_vec[1] != 1 || attr_vec[2] != 0)
        std::cerr << "Model Learner w/o Primary Attr Unit Test Failed!\n";
    std::unique_ptr<SquIDModel> a(learner.GetModel(0));
    std::unique_ptr<SquIDModel> b(learner.GetModel(1));
    std::unique_ptr<SquIDModel> c(learner.GetModel(2));
    if (a->GetTargetVar() != 0 || a->GetPredictorList().size() != 2 ||
        a->GetPredictorList()[0] != 1 || a->GetPredictorList()[1] != 2 || Check(a.get()) != 100)
        std::cerr << "Model Learner w/o Primary Attr Unit Test Failed!\n";
    if (b->GetTargetVar() != 1 || b->GetPredictorList().size() != 1 ||
        b->GetPredictorList()[0] != 2 || Check(b.get()) != 110)
        std::cerr << "Model Learner w/o Primary Attr Unit Test Failed!\n";
    if (c->GetTargetVar() != 2 || c->GetPredictorList().size() != 0 || Check(c.get()) != 111)
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
