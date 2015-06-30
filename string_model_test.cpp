#include "attribute.h"
#include "base.h"
#include "data_io.h"
#include "model.h"
#include "string_model.h"
#include "utility.h"

#include <cmath>
#include <vector>
#include <iostream>
#include <string>

namespace db_compress {

namespace {

Schema schema;
std::vector<std::unique_ptr<Tuple>> tuple;

void CreateTuple(Tuple* tuple, const std::string& a, const std::string& b) {
    TupleIStream istream(tuple, schema);
    istream << a << b;
}

void PrepareDB() {
    RegisterAttrValueCreator(0, new StringAttrValueCreator(), BASE_TYPE_STRING);
    std::vector<int> schema_; 
    for (int i = 0; i < 2; ++ i)
        schema_.push_back(0);
    schema = Schema(schema_);
    for (int i = 0; i < 10; ++ i) {
        std::unique_ptr<Tuple> ptr(new Tuple(2));
        tuple.push_back(std::move(ptr));
    }
    for (int i = 0; i < 4; ++ i) 
        CreateTuple(tuple[i].get(), "hello", "");
    CreateTuple(tuple[4].get(), "hello", "world");
    for (int i = 5; i < 8; ++ i)
        CreateTuple(tuple[i].get(), "hello", "a");
    CreateTuple(tuple[8].get(), "hello", "");
    CreateTuple(tuple[9].get(), "hello", "bc");
}

}  // anonymous namespace

void TestStringModelA() {
    Model* model = new StringModel(0);
    for (int i = 0; i < 10; ++ i)
        model->FeedTuple(*tuple[i]);
    if (model->GetPredictorList().size() != 0)
        std::cerr << "String Model Unit Test Failed!\n";
    if (model->GetTargetVar() != 0)
        std::cerr << "String Model Unit Test Failed!\n";
    model->EndOfData();
    if (model->GetModelDescriptionLength() != 8 + 255 * 16 + 63 * 8)
        std::cerr << "String Model Unit Test Failed!\n";
    {
        std::vector<size_t> blocks;
        blocks.push_back(8 + 255 * 16 + 63 * 8);
        ByteWriter writer(&blocks, "byte_writer_test.txt");
        model->WriteModel(&writer, 0);
    }
    std::ifstream fin("byte_writer_test.txt");
    std::vector<char> verify;
    char c;
    while (fin.get(c)) 
        verify.push_back(c);
    if (verify.size() != 574)
        std::cerr << "String Model Unit Test Failed!\n";
    if (verify[0] != Model::STRING_MODEL)
        std::cerr << "String Model Unit Test Failed!\n";
    for (int i = 1; i < 1 + 101 * 2; ++i )
    if (verify[i] != 0)
        std::cerr << "String Model Unit Test Failed!\n";
    for (int i = 203; i < 203 + 3 * 2; ++i )
    if (verify[i] != (char)51)
        std::cerr << "String Model Unit Test Failed!\n";
    for (int i = 209; i < 209 + 4 * 2; ++i )
    if (verify[i] != (char)102)
        std::cerr << "String Model Unit Test Failed!\n";
    for (int i = 217; i < 217 + 3 * 2; ++i )
    if (verify[i] != (char)204)
        std::cerr << "String Model Unit Test Failed!\n";
    for (int i = 223; i < 223 + 144 * 2; ++i )
    if (verify[i] != (char)255)
        std::cerr << "String Model Unit Test Failed!\n";
    for (int i = 511; i < 511 + 5; ++ i)
    if (verify[i] != 0)
        std::cerr << "String Model Unit Test Failed!\n";
    for (int i = 516; i < 516 + 58; ++ i)
    if (verify[i] != (char)255)
        std::cerr << "String Model Unit Test Failed!\n";
}

void TestStringModelB() {
    Model* model = new StringModel(1);
    for (int i = 0; i < 10; ++ i)
        model->FeedTuple(*tuple[i]);
    model->EndOfData();
    ProbInterval result(0, 1);
    std::vector<ProbInterval> vec;
    std::unique_ptr<AttrValue> attr;
    model->GetProbInterval(*tuple[8], &vec, &attr);
    result = ReducePIProduct(vec, NULL);
    if (fabs(result.l) > 0.01 || fabs(result.r - 0.5) > 0.01)
        std::cerr << "String Model Unit Test Failed!\n";
    if (attr != nullptr)
        std::cerr << "String Model Unit Test Failed!\n";
    vec.clear();
    model->GetProbInterval(*tuple[9], &vec, &attr);
    result = ReducePIProduct(vec, NULL);
    if (fabs(result.l - 0.834) > 0.001 || fabs(result.r - 0.835) > 0.001)
        std::cerr << "String Model Unit Test Failed!\n";
    if (attr != nullptr)
        std::cerr << "String Model Unit Test Failed!\n";    
}

void Test() {
    PrepareDB();
    TestStringModelA();
    TestStringModelB();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
