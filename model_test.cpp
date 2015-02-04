#include "attribute.h"
#include "base.h"
#include "data_io.h"
#include "model.h"

#include <cmath>
#include <vector>
#include <iostream>

namespace db_compress {

namespace {

Schema schema;
CompressionConfig config;
std::vector<std::unique_ptr<Tuple>> tuple;

void CreateTuple(Tuple* tuple, size_t a, size_t b) {
    TupleIStream istream(tuple, schema);
    istream << a << b;
}

void PrepareDB() {
    RegisterAttrValueCreator(0, new EnumAttrValueCreator(), BASE_TYPE_ENUM);
    std::vector<int> schema_; schema_.push_back(0); schema_.push_back(0); 
    schema = Schema(schema_);
    for (int i = 0; i < 10000; ++ i) {
        std::unique_ptr<Tuple> ptr(new Tuple(2));
        tuple.push_back(std::move(ptr));
    }
    for (int i = 0; i < 5000; ++ i) 
        CreateTuple(tuple[i].get(), 0, 0);
    for (int i = 5000; i < 8000; ++ i)
        CreateTuple(tuple[i].get(), 0, 1);
    for (int i = 8000; i < 10000; ++ i)
        CreateTuple(tuple[i].get(), 1, 1);
    config.allowed_err.push_back(0.01);
    config.allowed_err.push_back(0.01);
}

}  // anonymous namespace

void TestWithPrimaryAttr() {
    config.sort_by_attr = 1;
    ModelLearner learner(schema, config);
    while (1) {
        for (size_t i = 0; i < 10000; ++ i)
            learner.FeedTuple(*tuple[i]);
        learner.EndOfData();
        if (!learner.RequireMoreIterations())
            break;
    }
    std::vector<size_t> vec = learner.GetOrderOfAttributes();
    if (vec[0] != 1 || vec[1] != 0)
        std::cerr << "Model Learner w/ Primary Attr Unit Test Failed!\n";
    std::unique_ptr<Model> a(learner.GetModel(0));
    std::unique_ptr<Model> b(learner.GetModel(1));
    if (a->GetTargetVar() != 0 || 
        a->GetPredictorList().size() != 1 || 
        a->GetPredictorList()[0] != 1)
        std::cerr << "Model Learner w/ Primary Attr Unit Test Failed!\n";
    if (b->GetTargetVar() != 1 || b->GetPredictorList().size() != 0)
        std::cerr << "Model Learner w/ Primary Attr Unit Test Failed!\n";
    ProbInterval prob(0, 1);
    std::unique_ptr<AttrValue> attr;
    prob = b->GetProbInterval(*tuple[5000], prob, NULL, &attr);
    prob = a->GetProbInterval(*tuple[5000], prob, NULL, &attr);
    if (fabs(prob.l - 0.5)  > 0.05 || fabs(prob.r - 0.8) > 0.05)
        std::cerr << "Model Learner w/ Primary Attr Unit Test Failed!\n";
}

void TestWithoutPrimaryAttr() {
    config.sort_by_attr = -1;
    ModelLearner learner(schema, config);
    while (1) {
        for (size_t i = 0; i < 10000; ++ i)
            learner.FeedTuple(*tuple[i]);
        learner.EndOfData();
        if (!learner.RequireMoreIterations())
            break;
    }
    std::vector<size_t> vec = learner.GetOrderOfAttributes();
    if (vec[0] != 0 || vec[1] != 1)
        std::cerr << "Model Learner w/o Primary Attr Unit Test Failed!\n";
    std::unique_ptr<Model> a(learner.GetModel(0));
    std::unique_ptr<Model> b(learner.GetModel(1));
    if (a->GetTargetVar() != 0 || a->GetPredictorList().size() != 0)
        std::cerr << "Model Learner w/o Primary Attr Unit Test Failed!\n";
    if (b->GetTargetVar() != 1 ||
        b->GetPredictorList().size() != 1 ||
        b->GetPredictorList()[0] != 0)
        std::cerr << "Model Learner w/o Primary Attr Unit Test Failed!\n";
    ProbInterval prob(0, 1);
    std::unique_ptr<AttrValue> attr;
    prob = a->GetProbInterval(*tuple[5000], prob, NULL, &attr);
    prob = b->GetProbInterval(*tuple[5000], prob, NULL, &attr);
    if (fabs(prob.l - 0.5)  > 0.05 || fabs(prob.r - 0.8) > 0.05)
        std::cerr << "Model Learner w/ Primary Attr Unit Test Failed!\n";
}

void Test() {
    PrepareDB();
    TestWithPrimaryAttr();
    TestWithoutPrimaryAttr();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
