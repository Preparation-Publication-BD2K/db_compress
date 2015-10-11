#include "../base.h"
#include "../model.h"
#include "../numerical_model.h"

#include <memory>

class ColorAttr: public db_compress::AttrValue {
  private:
    double val_;
  public:
    ColorAttr(double val) : val_(val) {}
    double Value() const { return val_; }
};

class ColorProbTree: public db_compress::ProbTree {
  private:
    bool first_step_, is_zero_;
    db_compress::Prob zero_prob_;
    db_compress::LaplaceProbTree* tree_;
  public:
    ColorProbTree(db_compress::Prob zero_prob, 
                  db_compress::LaplaceProbTree* tree);
    bool HasNextBranch() const;
    void GenerateNextBranch();
    int GetNextBranch(const db_compress::AttrValue* attr) const;
    void ChooseNextBranch(int branch);
    db_compress::AttrValue* GetResultAttr() const;
};

class ColorModel: public db_compress::Model {
  private:
    std::unique_ptr<db_compress::Model> numeric_model_;
    std::unique_ptr<ColorProbTree> prob_tree_;
    size_t zero_count_, non_zero_count_;
    int model_cost_;
    db_compress::Prob zero_prob_;
    double err_;
  public:
    ColorModel(const db_compress::Schema& schema, const std::vector<size_t>& predictors, 
               size_t target_var, double err);
    db_compress::ProbTree* GetProbTree(const db_compress::Tuple& tuple);
    int GetModelCost() const { return model_cost_; }
    void FeedTuple(const db_compress::Tuple& tuple);
    void EndOfData();

    int GetModelDescriptionLength() const;
    void WriteModel(db_compress::ByteWriter* byte_writer, size_t block_index) const;
    static ColorModel* ReadModel(db_compress::ByteReader* byte_reader,
                                 const db_compress::Schema& schema, size_t index);
};

class ColorModelCreator: public db_compress::ModelCreator {
  public:
    db_compress::Model* ReadModel(db_compress::ByteReader* byte_reader,
                                  const db_compress::Schema& schema, size_t index);
    db_compress::Model* CreateModel(const db_compress::Schema& schema,
                                    const std::vector<size_t>& predictor,
                                    size_t target, double err);
};

class ColorInterpreter: public db_compress::AttrInterpreter {
  public:
    bool EnumInterpretable() const { return true; }
    int EnumCap() const;
    int EnumInterpret(const db_compress::AttrValue* attr) const;
    
    double NumericInterpretable() const { return true; }
    double NumericInterpret(const db_compress::AttrValue* attr) const {
        return static_cast<const ColorAttr*>(attr)->Value();
    }
};
