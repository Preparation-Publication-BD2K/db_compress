#include "attribute.h"
#include "base.h"
#include "data_io.h"
#include "model.h"
#include "categorical_model.h"

#include <vector>
#include <iostream>

namespace db_compress {

namespace {

Schema schema;
std::vector<std::unique_ptr<Tuple>> tuple;

void CreateTuple(Tuple* tuple, size_t a, size_t b, size_t c, size_t d) {
    TupleIStream istream(tuple, schema);
    istream << a << b << c << d;
}

void PrepareDB() {
    RegisterAttrValueCreator(0, new EnumAttrValueCreator(), BASE_TYPE_ENUM);
    std::vector<int> schema_; 
    for (int i = 0; i < 4; ++ i)
        schema_.push_back(0);
    schema = Schema(schema_);
    for (int i = 0; i < 10; ++ i) {
        std::unique_ptr<Tuple> ptr(new Tuple(4));
        tuple.push_back(std::move(ptr));
    }
    for (int i = 0; i < 4; ++ i) 
        CreateTuple(tuple[i].get(), 0, 0, 0, 0);
    CreateTuple(tuple[4].get(), 0, 0, 0, 1);
    for (int i = 5; i < 8; ++ i)
        CreateTuple(tuple[i].get(), 0, 1, 0, 1);
    CreateTuple(tuple[8].get(), 1, 1, 0, 1);
    CreateTuple(tuple[9].get(), 1, 1, 0, 0);
}

}  // anonymous namespace

void TestTableCategorical() {
    std::vector<size_t> pred; pred.push_back(0);
    Model* model = new TableCategorical(schema, pred, 1, 0.01);
    for (int i = 0; i < 10; ++ i)
        model->FeedTuple(*tuple[i]);
    if (model->GetPredictorList() != pred)
        std::cerr << "Categorical Model Unit Test Failed!\n";
    if (model->GetTargetVar() != 1)
        std::cerr << "Categorical Model Unit Test Failed!\n";
    model->EndOfData();
    if (model->GetModelDescriptionLength() != 16 + 16 + 16 + 40)
        std::cerr << "Categorical Model Description Length Unit Test Failed!\n";
    if (model->GetModelCost() != 88 + 7)
        std::cerr << "Categorical Model Cost Unit Test Failed!\n";
    {
        std::vector<size_t> blocks;
        blocks.push_back(88);
        ByteWriter writer(&blocks, "byte_writer_test.txt");
        model->WriteModel(&writer, 0);
    }
    std::ifstream fin("byte_writer_test.txt");
    std::vector<char> verify;
    char c;
    while (fin.get(c)) 
        verify.push_back(c);
    if (verify.size() != 11)
        std::cerr << "Categorical Model Unit Test Failed!\n";
    if (verify[0] != Model::TABLE_CATEGORY || verify[1] != 1 ||
        verify[2] != 8 || verify[3] != 0 || verify[4] != 0 ||
        verify[5] != 0 || verify[6] != 2 || verify[7] != 0 || verify[8] != 2 ||
        (int)((unsigned char)verify[9]) != 159 || verify[10] != 0)
        std::cerr << "Categorical Model Parameter Unit Test Failed!\n";
}

void TestTrivial() {
    std::vector<size_t> pred;
    Model* model = new TableCategorical(schema, pred, 2, 0);
    for (int i = 0; i < 10; ++ i)
        model->FeedTuple(*tuple[i]);
    model->EndOfData();
    if (model->GetModelDescriptionLength() != 40)
        std::cerr << "Trivial Categorical Model Unit Test Failed!\n";
    if (model->GetModelCost() != 40)
        std::cerr << "Trivial Categorical Model Unit Test Failed!\n";
}

void TestLossy() {
    std::vector<size_t> pred; pred.push_back(1);
    Model* model = new TableCategorical(schema, pred, 3, 0.25);
    for (int i = 0; i < 10; ++ i)
        model->FeedTuple(*tuple[i]);
    model->EndOfData();
    if (model->GetModelCost() != 88)
        std::cerr << "Lossy Categorical Model Unit Test Failed!\n";
    std::vector<ProbInterval> vec;
    std::unique_ptr<AttrValue> attr;
    model->GetProbInterval(*tuple[8], &vec, &attr);
    if (vec[0].l != 0 || vec[0].r != 1 || vec.size() != 1)
        std::cerr << "Lossy Categorical Model Unit Test Failed!\n";
    if (attr != nullptr)
        std::cerr << "Lossy Categorical Model Unit Test Failed!\n";
    model->GetProbInterval(*tuple[9], &vec, &attr);
    if (vec[1].l != 0 || vec[1].r != 1 || vec.size() != 2)
        std::cerr << "Lossy Categorical Model Unit Test Failed!\n";
    if (static_cast<EnumAttrValue*>(attr.get())->Value() != 1)
        std::cerr << "Lossy Categorical Model Unit Test Failed!\n";
}

void Test() {
    PrepareDB();
    TestTableCategorical();
    TestTrivial();
    TestLossy();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
