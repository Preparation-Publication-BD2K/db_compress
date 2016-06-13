#include "base.h"
#include "data_io.h"
#include "model.h"
#include "string_model.h"
#include "utility.h"

#include <vector>
#include <iostream>
#include <memory>
#include <string>

namespace db_compress {

Schema schema;
StringAttrValue str;
std::vector<size_t> pred;
Tuple tuple(1);

const Tuple& GetTuple(const std::string& s) {
    str.Set(s);
    tuple.attr[0] = &str;
    return tuple;
}

void PrepareData() {
    RegisterAttrModel(0, new StringModelCreator());
    std::vector<int> schema_; 
    schema_.push_back(0);
    schema = Schema(schema_);
}

void TestSquID() {
    std::unique_ptr<SquIDModel> model(GetAttrModel(0)[0]->CreateModel(schema, pred, 0, 0));
    model->FeedTuple(GetTuple(""));
    model->FeedTuple(GetTuple("aaa"));
    model->FeedTuple(GetTuple("aaaaaa"));
    model->FeedTuple(GetTuple("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                              "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    model->EndOfData();
    for (int len = 0; len < 100; ++len) {
        SquID* tree = model->GetSquID(GetTuple(""));
        if (!tree->HasNextBranch())
            std::cerr << "SquID Unit Test Failed!\n";
        tree->GenerateNextBranch();
        tree->ChooseNextBranch((len >= 63 ? 63 : len));
        for (int j = 0; j < len; ++j) {
            if (!tree->HasNextBranch())
                std::cerr << "SquID Unit Test Failed!\n";
            tree->GenerateNextBranch();
            if (tree->GetProbSegs()[0] != GetProb(504, 16))
                std::cerr << "SquID Unit Test Failed!\n";
            if (tree->GetProbSegs()[96] != GetProb(504, 16) ||
                tree->GetProbSegs()[97] != GetProb(65535, 16))
                std::cerr << "SquID Unit Test Failed!\n";
            tree->ChooseNextBranch(97);
        }
        if (tree->HasNextBranch() != (len >= 63))
            std::cerr << len << "SquID Unit Test Failed!\n";
    }
}

void TestModelCost() {
    std::unique_ptr<SquIDModel> model(GetAttrModel(0)[0]->CreateModel(schema, pred, 0, 0)); 
    model->EndOfData();
    if (model->GetModelDescriptionLength() != 255 * 16 + 63 * 8)
        std::cerr << "Model Cost Unit Test Failed!\n";
}

void TestModelDescription() {
    std::unique_ptr<SquIDModel> model(GetAttrModel(0)[0]->CreateModel(schema, pred, 0, 0));
    model->FeedTuple(GetTuple("aaa"));
    model->EndOfData();
    {
        std::vector<size_t> block;
        block.push_back(model->GetModelDescriptionLength());
        ByteWriter writer(&block, "byte_writer_test.txt");
        model->WriteModel(&writer, 0);
    }

    {
        ByteReader reader("byte_writer_test.txt");
        std::unique_ptr<SquIDModel> new_model(GetAttrModel(0)[0]->ReadModel(&reader, schema, 0));
        if (new_model->GetTargetVar() != 0 || new_model->GetPredictorList().size() != 0)
            std::cerr << "Model Description Unit Test Failed!\n";
        SquID* tree = new_model->GetSquID(GetTuple(""));
        tree->GenerateNextBranch();
        if (tree->GetProbSegs()[2] != GetZeroProb() || tree->GetProbSegs()[3] != GetProb(255, 8))
            std::cerr << "Model Description Unit Test Failed!\n";
        tree->ChooseNextBranch(3);
        tree->GenerateNextBranch();
        if (tree->GetProbSegs()[96] != GetZeroProb() || 
            tree->GetProbSegs()[97] != GetProb(65535, 16))
            std::cerr << "Model Description Unit Test Failed!\n";
    }    
}

void Test() {
    PrepareData();
    TestSquID();
    TestModelCost();
    TestModelDescription();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
