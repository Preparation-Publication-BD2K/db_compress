#include "base.h"
#include "model.h"
#include "compression.h"
#include "decompression.h"

#include <vector>
#include <iostream>

namespace db_compress {

Schema schema;
CompressionConfig config;

class MockAttr : public AttrValue {
  private:
    int value_;
  public:
    MockAttr(int val) : value_(val) {}
    void Set(int val) { value_ = val; }
    int Val() const { return value_; }
};

class MockProbTree : public ProbTree {
  private:
    bool first_step_;
    int branches_;
    int choice_;

    MockAttr attr_;
  public:
    MockProbTree(int branches) : first_step_(true), branches_(branches), attr_(0) {}
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
    const AttrValue* GetResultAttr() {
        attr_.Set(choice_);
        return &attr_;
    }
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
    int GetModelCost() const { return branch_; }
};

class MockModelCreator : public ModelCreator {
  public:
    Model* ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index) {
        int branch = byte_reader->ReadByte();
        return new MockModel(index, branch);
    }
    Model* CreateModel(const Schema& schema, const std::vector<size_t>& pred, size_t index,
                       double err) { return new MockModel(index, index + 1); }
};

void PrepareData() {
    RegisterAttrModel(0, new MockModelCreator());
    std::vector<int> schema_;
    for (int i = 0; i < 10; ++i)
        schema_.push_back(0);
    schema = Schema(schema_);
    config.allowed_err = std::vector<double>(10, 0);
    config.sort_by_attr = -1;
}

void TestRun() {
    int value[] = {0,1,1,2,1,4,6,0,3,3};
    std::vector<MockAttr> vec;
    Tuple tuple(10);
    for (int i = 0; i < 10; ++i) {
        vec.push_back(MockAttr(value[i]));
        tuple.attr[i] = &vec[i];
    }

    {
        Compressor compressor("compression_test.txt", schema, config);
        while (compressor.RequireMoreIterations()) {
            compressor.ReadTuple(tuple);
            compressor.EndOfData();
        }
    }
std::cerr << "Here\n";
    {
        Decompressor decompressor("compression_test.txt", schema);
        decompressor.Init();
        Tuple tuple_(10);
        decompressor.ReadNextTuple(&tuple);
        for (int i = 0; i < 10; i++)
        if (static_cast<const MockAttr*>(tuple_.attr[i])->Val() != value[i])
            std::cerr << "Test Run Failed!\n";
    }
}

void Test() {
    PrepareData();
    TestRun();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
