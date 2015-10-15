#include "base.h"
#include "compression.h"
#include "data_io.h"
#include "model.h"

#include <vector>
#include <iostream>

namespace db_compress {

const int signature = 101;

Schema schema;
CompressionConfig config;
Tuple tuple(2);

class MockProbTree : public ProbTree {
  private:
    bool first_step_;
    int type_;
  public:
    MockProbTree(int type) : first_step_(true), type_(type) {}
    bool HasNextBranch() const { return first_step_; }
    void GenerateNextBranch() { 
        prob_segs_.clear();
        prob_segs_.push_back(GetProb(1,1));
    }
    int GetNextBranch(const AttrValue* attr) const { return type_; }
    void ChooseNextBranch(int branch) { first_step_ = false; }
    const AttrValue* GetResultAttr() { return NULL; }
};

class MockModel : public Model {
  private:
    std::unique_ptr<ProbTree> prob_tree_;
  public:
    MockModel(const std::vector<size_t>& pred, size_t target) : Model(pred, target) {}
    ProbTree* GetProbTree(const Tuple& tuple) { 
        prob_tree_.reset(new MockProbTree(target_var_));
        return prob_tree_.get();
    }
    int GetModelDescriptionLength() const { return 8; }
    void WriteModel(ByteWriter* byte_writer, size_t block_index) const { 
        byte_writer->WriteByte(signature + target_var_, block_index);
    }
    int GetModelCost() const { return target_var_; }
};

class MockModelCreator : public ModelCreator {
  public:
    Model* ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index) { return NULL; }
    Model* CreateModel(const Schema& schema, const std::vector<size_t>& pred, 
                       size_t index, double err) {
        return new MockModel(pred, index);
    }
};

void PrepareData() {
    RegisterAttrModel(0, new MockModelCreator());
    std::vector<int> schema_; schema_.push_back(0); schema_.push_back(0); 
    schema = Schema(schema_);
    config.allowed_err.push_back(0.01);
    config.allowed_err.push_back(0.01);
    config.sort_by_attr = -1;
}

void TestCompression() {
    Compressor compressor("compression_test.txt", schema, config);
    while (1) {
        compressor.ReadTuple(tuple);
        compressor.ReadTuple(tuple);
        compressor.EndOfData();
        if (!compressor.RequireMoreIterations())
            break;
    }

    std::ifstream fin("compression_test.txt");
    char c;
    std::vector<unsigned char> file;
    while (fin.get(c)) {
        file.push_back((unsigned char) c);
    }
    unsigned char correct_answer[] = {1, 0, 0, 0, 1, 0, 101, 0, 102, 0x5c};
    if (file.size() != 10)
        std::cerr << "Compression Unit Test Failed!\n";
    for (int i = 0; i < 10; ++i)
    if (file[i] != correct_answer[i])
        std::cerr << "Compression Unit Test Failed!\n";
}

void Test() {
    PrepareData();
    TestCompression();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
