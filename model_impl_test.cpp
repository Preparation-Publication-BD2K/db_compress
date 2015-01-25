#include "attribute.h"
#include "base.h"
#include "data_io.h"
#include "model.h"
#include "model_impl.h"
#include <vector>
#include <iostream>

namespace db_compress {

namespace {

Schema schema;
std::vector<std::unique_ptr<Tuple>> tuple;

void CreateTuple(Tuple* tuple, size_t a, size_t b, size_t c) {
    TupleIStream istream(tuple, schema);
    istream << a << b << c;
}

void PrepareDB() {
    RegisterAttrValueCreator(0, new EnumAttrValueCreator(), BASE_TYPE_ENUM);
    std::vector<int> schema_; 
    schema_.push_back(0); schema_.push_back(0); schema_.push_back(0);
    schema = Schema(schema_);
    for (int i = 0; i < 10; ++ i) {
        std::unique_ptr<Tuple> ptr(new Tuple(3));
        tuple.push_back(std::move(ptr));
    }
    for (int i = 0; i < 5; ++ i) 
        CreateTuple(tuple[i].get(), 0, 0, 0);
    for (int i = 5; i < 8; ++ i)
        CreateTuple(tuple[i].get(), 0, 1, 0);
    for (int i = 8; i < 10; ++ i)
        CreateTuple(tuple[i].get(), 1, 1, 0);
}

}  // anonymous namespace

void TestDynamicList() {
    std::vector<size_t> index;
    index.push_back(0); index.push_back(1); index.push_back(2);
    DynamicList<int> dynamic_list;
    dynamic_list[index] = 1;
    index[2] = 3;
    dynamic_list[index] = 2;
    if (dynamic_list.size() != 2)
        std::cerr << "Dynamic List Unit Test Failed!\n";
    if (dynamic_list[index] != 2 || dynamic_list[1] != 2)
        std::cerr << "Dynamic List Unit Test Failed!\n";
    const DynamicList<int>& another = dynamic_list;
    if (another[index] != 2)
        std::cerr << "Dynamic List Unit Test Failed!\n";
    index[2] = 2;
    if (dynamic_list[index] != 1)
        std::cerr << "Dynamic List Unit Test Failed!\n";
    dynamic_list[index] = 3;
    if (dynamic_list.size() != 2 || another[index] != 3)
        std::cerr << "Dynamic List Unit Test Failed!\n";   
}

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
    if (model->GetModelDescriptionLength() != 16 + 8 + 8 + 24)
        std::cerr << "Categorical Model Description Length Unit Test Failed!\n";
    if (model->GetModelCost() != 56 + 7)
        std::cerr << "Categorical Model Cost Unit Test Failed!\n";
    {
        std::vector<size_t> blocks;
        blocks.push_back(56);
        ByteWriter writer(&blocks, "byte_writer_test.txt");
        model->WriteModel(&writer, 0);
    }
    std::ifstream fin("byte_writer_test.txt");
    std::vector<char> verify;
    char c;
    while (fin.get(c)) 
        verify.push_back(c);
    if (verify.size() != 7)
        std::cerr << "Categorical Model Unit Test Failed!\n";
    if (verify[0] != Model::TABLE_CATEGORY || verify[1] != 1 ||
        verify[2] != 8 || verify[3] != 0 || verify[4] != 2 || 
        (int)((unsigned char)verify[5]) != 159 || verify[6] != 0)
        std::cerr << "Categorical Model Parameter Unit Test Failed!\n";
}

void TestTrivial() {
    std::vector<size_t> pred;
    Model* model = new TableCategorical(schema, pred, 2, 0);
    for (int i = 0; i < 10; ++ i)
        model->FeedTuple(*tuple[i]);
    model->EndOfData();
    if (model->GetModelDescriptionLength() != 24)
        std::cerr << "Trivial Categorical Model Unit Test Failed!\n";
    if (model->GetModelCost() != 24)
        std::cerr << "Trivial Categorical Model Unit Test Failed!\n";
}

void Test() {
    TestDynamicList();
    PrepareDB();
    TestTableCategorical();
    TestTrivial();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
