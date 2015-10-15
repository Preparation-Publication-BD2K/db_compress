#include "base.h"
#include "utility.h"
#include "model.h"

#include <cmath>
#include <vector>
#include <iostream>

namespace db_compress {

class MockAttr : public AttrValue {
  private:
    int val_;
  public:
    MockAttr(int val) : val_(val) {}
    int Val() const { return val_; }
};

class MockProbTree : public ProbTree {
  private:
    int step_;
    int val_;
    MockAttr attr_;
  public:
    MockProbTree() : step_(0), val_(0), attr_(0) { prob_segs_.resize(1); }
    bool HasNextBranch() const { return step_ < 3; }
    void GenerateNextBranch() { prob_segs_[0] = GetProb(1, step_); }
    int GetNextBranch(const AttrValue* attr) const {
        int val = static_cast<const MockAttr*>(attr)->Val();
        return (val >> (2 - step_)) & 1;
    }
    void ChooseNextBranch(int branch) {val_ = val_ * 2 + branch; ++ step_; }
    const AttrValue* GetResultAttr() {
        attr_ = MockAttr(val_);
        return &attr_;
    }
};

void TestProbTree() {
    MockProbTree tree;
    tree.ChooseNextBranch(0);
    tree.GenerateNextBranch();
    if (tree.GetProbInterval(0).l != GetZeroProb() || 
        tree.GetProbInterval(0).r != GetProb(1, 1))
        std::cerr << "Prob Tree Unit Test Failed!\n";
    if (tree.GetProbInterval(1).l != GetProb(1, 1) || 
        tree.GetProbInterval(1).r != GetOneProb())
        std::cerr << "Prob Tree Unit Test Failed!\n";
}

void TestDecoder() {
    std::vector<int> stop_time(8, -1), result(8, -1);
    std::vector<ProbInterval> resultPIt;
    std::vector<UnitProbInterval> resultPIb;
    for (int i = 0; i < 8; ++i) {
        resultPIt.push_back(ProbInterval(GetZeroProb(), GetZeroProb()));
        resultPIb.push_back(UnitProbInterval(0, 0));
    }
    ProbInterval PIt(GetProb(3, 3), GetProb(6, 3));
    UnitProbInterval PIb(2, 2);
    for (int i = 0; i < 8; ++i) {
        MockProbTree tree;
        Decoder decoder;
        decoder.Init(&tree, PIt, PIb);
        for (int j = 0; j < 4; ++j) {
            if (decoder.IsEnd()) {
                stop_time[i] = j;
                const MockAttr* attr = static_cast<const MockAttr*>(decoder.GetResult());
                result[i] = attr->Val();
                resultPIt[i] = decoder.GetPIt();
                resultPIb[i] = decoder.GetPIb();
                break;
            } else if (j != 3)
                decoder.FeedBit((i >> (2 - j)) & 1);
        }
    }
    int true_stop_time[8] = {2, 2, 3, -1, 1, 1, 1, 1};
    int true_result[8] = {1, 1, 2, -1, 3, 3, 3, 3};
    for (int i = 0; i < 8; ++i)
    if (stop_time[i] != true_stop_time[i] || result[i] != true_result[i])
        std::cerr << "Decoder Unit Test Failed!\n";
    int true_PIt_l[8] = {27, 27, 36, 0, 39, 39, 39, 39};
    int true_PIt_r[8] = {36, 36, 39, 0, 48, 48, 48, 48};
    int true_PIb_num[8] = {8, 8, 18, 0, 5, 5, 5, 5};
    int true_PIb_exp[8] = {4, 4, 5, 0, 3, 3, 3, 3};
    for (int i = 0; i < 8; ++i)
    if (resultPIt[i].l != GetProb(true_PIt_l[i], 6) ||
        resultPIt[i].r != GetProb(true_PIt_r[i], 6) ||
        resultPIb[i].num != true_PIb_num[i] ||
        resultPIb[i].exp != true_PIb_exp[i])
        std::cerr << "Decoder Unit Test Failed!\n";
}

void Test() {
    TestProbTree();
    TestDecoder();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
