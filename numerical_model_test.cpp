#include "base.h"
#include "data_io.h"
#include "model.h"
#include "utility.h"
#include "numerical_model.h"

#include <cmath>
#include <vector>
#include <memory>
#include <iostream>

namespace db_compress {

class MockAttr : public AttrValue {
  private:
    int val_;
  public:
    void Set(int val) { val_ = val; }
    int Val() const { return val_; }
};

class MockInterpreter : public AttrInterpreter {
  public:
    bool EnumInterpretable() const { return true; }
    int EnumCap() const { return 3; }
    int EnumInterpret(const AttrValue* attr) const {
        return static_cast<const MockAttr*>(attr)->Val();
    }
};

Schema schema;
MockAttr attr;
IntegerAttrValue num_attr;
std::vector<size_t> pred;
Tuple tuple(2);

const Tuple& GetTuple(size_t a, int b) {
    attr.Set(a);
    num_attr.Set(b);
    tuple.attr[0] = &attr;
    tuple.attr[1] = &num_attr;
    return tuple;
}

void PrepareData() {
    RegisterAttrModel(1, new TableLaplaceIntCreator());
    std::vector<int> schema_(2);
    schema = Schema(schema_);
    pred.push_back(0);
    RegisterAttrInterpreter(0, new MockInterpreter());
}

void TestProbTree() {
    std::unique_ptr<Model> model(GetAttrModel(1)[0]->CreateModel(schema, pred, 1, 0.1));
    for (int i = -2; i <= 2; ++i)
        model->FeedTuple(GetTuple(0, i));
    model->FeedTuple(GetTuple(0, 0));
    for (int i = 0; i < 3; ++i)
        model->FeedTuple(GetTuple(1, (int)0));
    model->EndOfData();
    int test_a[9] = {0, 0, 0, 0, 0, 0, 0, 0, 1};
    int test_branch[9] = {0, 1, 2, 3, 11, 12, 31, 32, 0};
    bool test_next_branch[9] = {true, true, false, true, true, false, false, true, false};
    int branch_prob[9] = {19875, 24109, 0, 65536 - 24109, 24109, 0, 0, 65536 - 24109, 0};
    int boundary[9] = {0, -1, 0, 2, -2, 0, 0, 3, 0};  
    int result[9] = {0, 0, 0, 0, 0, -1, 1, 0, 0};
    for (int i = 0; i < 9; ++i) {
        ProbTree* tree = model->GetProbTree(GetTuple(test_a[i], 0));
        int branch = test_branch[i];
        std::vector<int> branches;
        while (branch > 0) {
            branches.push_back(branch % 10 - 1);
            branch /= 10;
        }
        for (int j = branches.size() - 1; j >= 0; --j) {
            if (!tree->HasNextBranch())
                std::cerr << "ProbTree Unit Test Failed!\n";
            tree->GenerateNextBranch();
            tree->ChooseNextBranch(branches[j]);
        }
        if (tree->HasNextBranch() != test_next_branch[i])
            std::cerr << "ProbTree Unit Test Failed!\n";
        if (tree->HasNextBranch()) {
            tree->GenerateNextBranch();
            Prob prob = tree->GetProbSegs()[0];
            if (prob != GetProb(branch_prob[i], 16))
                std::cerr << "ProbTree Unit Test Failed!\n";
            IntegerAttrValue attr(boundary[i]), l_attr(boundary[i] - 1);
            if (tree->GetNextBranch(&attr) != 1 ||
                tree->GetNextBranch(&l_attr) != 0)
                std::cerr << "ProbTree Unit Test Failed!\n";
        } else {
            const AttrValue* attr(tree->GetResultAttr());
            if (static_cast<const IntegerAttrValue*>(attr)->Value() != result[i])
                std::cerr << "ProbTree Unit Test Failed!\n";
        }
    }
}

void TestModelCost() {
    std::unique_ptr<Model> model(GetAttrModel(1)[0]->CreateModel(schema, pred, 1, 0.1));
    for (int i = -1; i <= 1; ++ i)
        model->FeedTuple(GetTuple(0, i * 100));
    model->FeedTuple(GetTuple(0, 0));
    model->EndOfData();
    if (model->GetModelDescriptionLength() != 248)
        std::cerr << "Model Cost Unit Test Failed!\n";
    if (model->GetModelCost() != 280)
        std::cerr << "Model Cost Unit Test Failed!\n";
}

void TestModelDescription() {
    std::unique_ptr<Model> model(GetAttrModel(1)[0]->CreateModel(schema, pred, 1, 0.1));
    for (int i = -2; i <= 2; ++i)
        model->FeedTuple(GetTuple(0, i));
    model->FeedTuple(GetTuple(0, 0));
    for (int i = 0; i < 3; ++i)
        model->FeedTuple(GetTuple(1, (int)0));
    for (int i = -2; i <= 2; ++i)
        model->FeedTuple(GetTuple(2, i * 100));
    model->FeedTuple(GetTuple(2, 0));
    model->EndOfData();
    {
        std::vector<size_t> block;
        block.push_back(model->GetModelDescriptionLength());
        ByteWriter writer(&block, "byte_writer_test.txt");
        model->WriteModel(&writer, 0);
    }

    {
        ByteReader reader("byte_writer_test.txt");
        std::unique_ptr<Model> new_model(GetAttrModel(1)[0]->ReadModel(&reader, schema, 1));
        if (new_model->GetTargetVar() != 1 || new_model->GetPredictorList().size() != 1 ||
            new_model->GetPredictorList()[0] != 0)
            std::cerr << "Model Description Unit Test Failed!\n";
        int sdev[3] = {1, 0, 100};
        for (int i = 0; i < 3; ++i) {
            int dev = sdev[i];
            ProbTree* tree = new_model->GetProbTree(GetTuple(i, 0));
            if (dev == 0) {
                if (tree->HasNextBranch())
                    std::cerr << "Model Description Unit Test Failed!\n";
            } else {
                tree->GenerateNextBranch();
                tree->ChooseNextBranch(2);
                tree->GenerateNextBranch();
                IntegerAttrValue l(dev), r(dev + 1);
                if (tree->GetNextBranch(&l) != 0 || tree->GetNextBranch(&r) != 1)
                    std::cerr << "Model Description Unit Test Failed!\n";
            }
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
