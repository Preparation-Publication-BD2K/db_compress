#include "base.h"
#include "model.h"
#include "utility.h"
#include "categorical_model.h"

#include <vector>
#include <iostream>
#include <memory>

namespace db_compress {

Schema schema;
std::vector<EnumAttrValue> vec;
std::vector<size_t> pred;
Tuple tuple(3);

class MockInterpreter : public AttrInterpreter {
  public:
    bool EnumInterpretable() const { return true; }
    int EnumCap() const { return 2; }
    int EnumInterpret(const AttrValue* attr) const {
        return static_cast<const EnumAttrValue*>(attr)->Value();
    }
};

const Tuple& GetTuple(size_t a, size_t b, size_t c) {
    vec.clear();
    vec.push_back(EnumAttrValue(a));
    vec.push_back(EnumAttrValue(b));
    vec.push_back(EnumAttrValue(c));
    for (int i = 0; i < 3; ++i)
        tuple.attr[i] = &vec[i];
    return tuple;
}

void PrepareData() {
    RegisterAttrModel(0, new TableCategoricalCreator());
    std::vector<int> schema_; 
    for (int i = 0; i < 3; ++ i)
        schema_.push_back(0);
    schema = Schema(schema_);
    pred.push_back(0); pred.push_back(1);
    RegisterAttrInterpreter(0, new MockInterpreter());
}

void TestProbTree() {
    std::unique_ptr<Model> model(new TableCategorical(schema, pred, 2, 0.1));
    for (int i = 0; i < 10; ++ i)
        model->FeedTuple(GetTuple(0, 0, 0));
    model->FeedTuple(GetTuple(0, 0, 3));
    model->FeedTuple(GetTuple(0, 1, 0));
    model->FeedTuple(GetTuple(0, 1, 3));
    for (int i = 0; i < 3; ++ i)
        model->FeedTuple(GetTuple(1, 0, 0));
    model->FeedTuple(GetTuple(1, 0, 3));
    for (int i = 0; i < 4; ++ i)
        model->FeedTuple(GetTuple(1, 1, i));
    model->EndOfData();
    int test_a[4] = {0, 0, 1, 1};
    int test_b[4] = {0, 1, 0, 1};
    Prob test_ans[4][3] = { {GetProb(255, 8), GetProb(255, 8), GetProb(255, 8)},
                            {GetProb(1,1), GetProb(1,1), GetProb(1,1)},
                            {GetProb(3,2), GetProb(3,2), GetProb(3,2)},
                            {GetProb(1,2), GetProb(2,2), GetProb(3,2)} };
    int test_ret[4] = {0, 0, 0, 3};
    for (int i = 0; i < 4; ++ i) {
        ProbTree* tree = model->GetProbTree(GetTuple(test_a[i], test_b[i], i));
        if (!tree->HasNextBranch())
            std::cerr << "Prob Tree Unit Test Failed!\n";
        if (tree->GetProbSegs().size() != 3)
            std::cerr << "Prob Tree Unit Test Failed!\n";
        for (int j = 0; j < 3; ++ j)
        if (tree->GetProbSegs()[j] != test_ans[i][j])
            std::cerr << "Prob Tree Unit Test Failed!\n";
        if (tree->GetNextBranch(tuple.attr[2]) != test_ret[i])
            std::cerr << "Prob Tree Unit Test Failed!\n";
        tree->ChooseNextBranch(i);
        if (tree->HasNextBranch())
            std::cerr << "Prob Tree Unit Test Failed!\n";
    }
}

void TestModelCost() {
    std::unique_ptr<Model> model(GetAttrModel(0)[0]->CreateModel(schema, pred, 2, 0));
    for (int i = 0; i < 2; ++ i)
    for (int j = 0; j < 2; ++ j)
    for (int k = 0; k < 2; ++ k)
        model->FeedTuple(GetTuple(i, j, k));
    model->EndOfData();
    if (model->GetModelDescriptionLength() != 96)
        std::cerr << "Model Cost Unit Test Failed!\n";
    if (model->GetModelCost() != 104)
        std::cerr << "Model Cost Unit Test Failed!\n";
}

void TestModelDescription() {
    std::unique_ptr<Model> model(GetAttrModel(0)[0]->CreateModel(schema, pred, 2, 0));
    for (int i = 0; i < 2; ++ i)
    for (int j = 0; j < 2; ++ j)
    for (int k = 0; k < 2; ++ k)
        model->FeedTuple(GetTuple(i, j, k));
    model->EndOfData();
    {
        std::vector<size_t> block;
        block.push_back(model->GetModelDescriptionLength());
        ByteWriter writer(&block, "byte_writer_test.txt");
        model->WriteModel(&writer, 0);
    }

    {
        ByteReader reader("byte_writer_test.txt");
        std::unique_ptr<Model> new_model(GetAttrModel(0)[0]->ReadModel(&reader, schema, 2));
        if (new_model->GetTargetVar() != 2 || new_model->GetPredictorList().size() != 2 ||
            new_model->GetPredictorList()[0] != 0 ||
            new_model->GetPredictorList()[1] != 1)
            std::cerr << "Model Description Unit Test Failed!\n";
        for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j) {
            ProbTree* tree = new_model->GetProbTree(GetTuple(i, j, 0));
            if (tree->GetProbSegs().size() != 1 ||
                tree->GetProbSegs()[0] != GetProb(1, 1))
                std::cerr << "Model Description Unit Test Failed!\n";
        }
    }
}

void Test() {
    PrepareData();
    TestProbTree();
    TestModelCost();
    TestModelDescription();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
