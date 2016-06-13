#include "base.h"
#include "model.h"

namespace db_compress {

class MockAttr : public AttrValue {
  private:
    int value_;
  public:
    MockAttr(int val) : value_(val) {}
    void Set(int val) { value_ = val; }
    int Val() const { return value_; }
};

class MockSquID : public SquID {
  private:
    bool first_step_;
    int branches_;
    int choice_;

    MockAttr attr_;
  public:
    MockSquID(int branches) : first_step_(true), branches_(branches), attr_(0) {}
    bool HasNextBranch() const { return first_step_; }
    void GenerateNextBranch() {
        prob_segs_.clear();
        for (int i = 1; i < branches_; ++i)
            prob_segs_.push_back(GetProb(65536 * i / branches_, 16));
    }
    int GetNextBranch(const AttrValue* attr) const {
        return static_cast<const MockAttr*>(attr)->Val();
    }
    void ChooseNextBranch(int branch) {
        first_step_ = false;
        choice_ = branch;
    }
    const AttrValue* GetResultAttr() {
        attr_.Set(choice_);
        return &attr_;
    }
};

class MockModel : public SquIDModel {
  private:
    int branch_;
    std::unique_ptr<SquID> squid_;
  public:
    MockModel(const std::vector<size_t>& pred, size_t target_var, int branch) : 
      SquIDModel(pred, target_var), 
      branch_(branch) {}
    SquID* GetSquID(const Tuple& tuple) {
        squid_.reset(new MockSquID(branch_));
        return squid_.get();
    }
    int GetModelDescriptionLength() const { return 8; }
    void WriteModel(ByteWriter* byte_writer, size_t block_index) const {
        byte_writer->WriteByte(branch_, block_index);
    }
    int GetModelCost() const { return branch_; }
};

class MockModelCreator : public ModelCreator {
  private:
    int range_;
  public:
    MockModelCreator(int range) : range_(range) {}
    SquIDModel* ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index) {
        int branch = byte_reader->ReadByte();
        return new MockModel(std::vector<size_t>(), index, branch);
    }
    SquIDModel* CreateModel(const Schema& schema, const std::vector<size_t>& pred, 
                            size_t index, double err) { 
        return new MockModel(pred, index, range_); 
    }
};

} // namespace db_compress
