#include "base.h"
#include "data_io.h"
#include "model.h"
#include "decompression.h"

#include <vector>
#include <memory>
#include <iostream>

namespace db_compress {

Schema schema;

class MockAttr : public AttrValue {
  private:
    int value_;
  public:
    MockAttr(int val) : value_(val) {}
    int Val() const { return value_; }
};

class MockProbTree : public ProbTree {
  private:
    bool first_step_;
    int branches_;
    int choice_;
  public:
    MockProbTree(int branches) : first_step_(true), branches_(branches) {}
    bool HasNextBranch() const { return first_step_; }
    void GenerateNextBranch() {
        prob_segs_.clear();
        for (int i = 1; i < branches_; ++i)
            prob_segs_.push_back(GetProb(65536 * i / branches_, 16));
    }
    int GetNextBranch(const AttrValue* attr) const { return 0; }
    void ChooseNextBranch(int branch) {
        first_step_ = false;
        choice_ = branch;
    }
    AttrValue* GetResultAttr() const { return new MockAttr(choice_); }
    
};

class MockModel : public Model {
  private:
    int branch_;
    std::unique_ptr<ProbTree> prob_tree_;
  public:
    MockModel(size_t target_var, int branch) : 
      Model(std::vector<size_t>(), target_var), 
      branch_(branch) {}
    ProbTree* GetProbTree(const Tuple& tuple) {
        prob_tree_.reset(new MockProbTree(branch_));
        return prob_tree_.get();
    }
    int GetModelDescriptionLength() const { return 0; }
    void WriteModel(ByteWriter* byte_writer, size_t block_index) const {}
    int GetModelCost() const { return 0; }
};

class MockModelCreator : public ModelCreator {
  public:
    Model* ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index) {
        int branch = byte_reader->ReadByte();
        return new MockModel(index, branch);
    }
    Model* CreateModel(const Schema& schema, const std::vector<size_t>& pred, size_t index,
                       double err) { return NULL; }
};

void PrepareData() {
    RegisterAttrModel(0, new MockModelCreator());
    std::vector<int> schema_; schema_.push_back(0); schema_.push_back(0);
    schema = Schema(schema_);
    std::ofstream fout("compression_test.txt");
    unsigned char data[] = {3,0,0,0,1,0,3,0,6,0x81,0xfc};
    for (int i = 0; i < 11; ++i )
        fout << data[i];
    fout.close();
}

void TestDecompression() {
    Decompressor decompressor("compression_test.txt", schema);
    decompressor.Init();
    for (int i = 0; i < 2; ++i) {
        ResultTuple tuple;
        if (!decompressor.HasNext())
            std::cerr << "Decompression Unit Test Failed!\n";
        decompressor.ReadNextTuple(&tuple);
        TupleOStream ostream(tuple);
        AttrValue* vec[2];
        ostream >> vec[0] >> vec[1];
        if (static_cast<const MockAttr*>(vec[0])->Val() != 0 ||
            static_cast<const MockAttr*>(vec[1])->Val() != 2)
            std::cerr << "Decompression Unit Test Failed!\n";
    }
    if (decompressor.HasNext())
        std::cerr << "Decompression Unit Test Failed!\n";
}

void Test() {
    PrepareData();
    TestDecompression();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
