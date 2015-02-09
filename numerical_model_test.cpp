#include "attribute.h"
#include "base.h"
#include "data_io.h"
#include "model.h"
#include "numerical_model.h"

#include <cmath>
#include <vector>
#include <memory>
#include <iostream>

namespace db_compress {

namespace {

Schema schema;
std::vector<std::unique_ptr<Tuple>> tuple;

void CreateTuple(Tuple* tuple, size_t a, int b, double c, int d) {
    TupleIStream istream(tuple, schema);
    istream << a << b << c << d;
}

void PrepareDB() {
    RegisterAttrValueCreator(0, new EnumAttrValueCreator(), BASE_TYPE_ENUM);
    RegisterAttrValueCreator(1, new IntegerAttrValueCreator(), BASE_TYPE_INTEGER);
    RegisterAttrValueCreator(2, new DoubleAttrValueCreator(), BASE_TYPE_DOUBLE);
    std::vector<int> schema_;
    schema_.push_back(0); schema_.push_back(1); 
    schema_.push_back(2); schema_.push_back(1);
    schema = Schema(schema_);
    for (int i = 0; i < 10; ++ i ) {
        std::unique_ptr<Tuple> ptr(new Tuple(4));
        tuple.push_back(std::move(ptr));
    }
    CreateTuple(tuple[0].get(), 0, 0, 0, 0);
    CreateTuple(tuple[1].get(), 0, 0, 0.5, 0);
    CreateTuple(tuple[2].get(), 0, 1, 1, 0);
    CreateTuple(tuple[3].get(), 0, 1, 1.5, 0);
    CreateTuple(tuple[4].get(), 0, 1, 2, 0);
    CreateTuple(tuple[5].get(), 1, 0, 2.5, 0);
    CreateTuple(tuple[6].get(), 1, 1, 3, 0);
    CreateTuple(tuple[7].get(), 1, 1, 3.5, 0);
    CreateTuple(tuple[8].get(), 1, 1, 4, 0);
    CreateTuple(tuple[9].get(), 1, 2, 4.5, 0);
}

}  // anonymous namespace

void TestTableLaplaceDouble() {
    std::vector<size_t> pred; pred.push_back(0);
    Model* model = new TableLaplace(schema, pred, 2, false, 0.1);
    for (int i = 0; i < 10; ++ i)
        model->FeedTuple(*tuple[i]);
    if (model->GetPredictorList() != pred)
        std::cerr << "Laplace Model Unit Test Failed!\n";
    if (model->GetTargetVar() != 2)
        std::cerr << "Laplace Model Unit Test Failed!\n";
    model->EndOfData();
    if (model->GetModelDescriptionLength() != 16 + 16 + 16 + 64 * 2)
        std::cerr << "Laplace Model Unit Test Failed!\n";
    if (model->GetModelCost() != 176 + 25)
        std::cerr << "Laplace Model Unit Test Failed!\n";
    {
        std::vector<size_t> blocks;
        blocks.push_back(96);
        ByteWriter writer(&blocks, "byte_writer_test.txt");
        model->WriteModel(&writer, 0);
    }
    std::ifstream fin("byte_writer_test.txt");
    std::vector<char> verify;
    char c;
    while (fin.get(c))
        verify.push_back(c);
    if (verify.size() != 22)
        std::cerr << "Laplace Model Unit Test Failed!\n";
    // Model Meta Data
    if (verify[0] != Model::TABLE_LAPLACE || verify[1] != 1 ||
        verify[2] != 0 || verify[3] != 0 || verify[4] != 0 || verify[5] != 2)
        std::cerr << "Laplace Model Unit Test Failed!\n";
    // First set of parameters
    if (verify[6] != 0x3f || verify[7] != (char)0x80 || verify[8] != 0 || verify[9] != 0 ||
        verify[10] != 0x3f || verify[11] != (char)0x19 || 
        verify[12] != (char)0x99 || verify[13] != (char)0x9a)
        std::cerr << "Laplace Model Unit Test Failed!\n";
    if (verify[14] != 0x40 || verify[15] != 0x60 || verify[16] != 0 || verify[17] != 0 ||
        verify[18] != 0x3f || verify[19] != (char)0x19 || 
        verify[20] != (char)0xc99|| verify[21] != (char)0x9a)
        std::cerr << "Laplace Model Unit Test Failed!\n";
    ProbInterval prob(0, 1);
    std::unique_ptr<AttrValue> attr(nullptr);
    prob = model->GetProbInterval(*tuple[0], prob, NULL, &attr);
    if (fabs(prob.l - 0.09375) > 0.00001 || fabs(prob.r - 0.125) > 0.00001)
        std::cerr << "Laplace Model Unit Test Failed!\n";
    if (fabs(static_cast<DoubleAttrValue*>(attr.get())->Value()) > 0.1)
        std::cerr << "Laplace Model Unit Test Failed!\n";
}

void TestTableLaplaceInt() {
    std::vector<size_t> pred; pred.push_back(0);
    Model* model = new TableLaplace(schema, pred, 1, true, 0.1);
    for (int i = 0; i < 10; ++ i)
        model->FeedTuple(*tuple[i]);
    model->EndOfData();
    ProbInterval prob(0, 1);
    std::unique_ptr<AttrValue> attr(nullptr);
    prob = model->GetProbInterval(*tuple[0], prob, NULL, &attr);
    if (fabs(prob.l - 0.125) > 0.001 || fabs(prob.r - 0.25) > 0.001)
        std::cerr << "Laplace Integer Model Unit Test Failed!\n";
    if (fabs(static_cast<IntegerAttrValue*>(attr.get())->Value()) > 0.1)
        std::cerr << "Laplace Integer Model Unit Test Failed!\n";
}

void TestTrivial() {
    std::vector<size_t> pred;
    Model* model = new TableLaplace(schema, pred, 3, true, 0.1);
    for (int i = 0; i < 10; ++ i)
        model->FeedTuple(*tuple[i]);
    model->EndOfData();
    ProbInterval prob(0, 1);
    std::unique_ptr<AttrValue> attr(nullptr);
    prob = model->GetProbInterval(*tuple[0], prob, NULL, &attr);
    if (prob.l != 0 || prob.r != 1)
        std::cerr << "Laplace Trivial Model Unit Test Failed!\n";
    if (attr != nullptr)
        std::cerr << "Laplace Trivial Model Unit Test Failed!\n";
}

void Test() {
    PrepareDB();
    TestTableLaplaceDouble();
    TestTableLaplaceInt();
    TestTrivial();    
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
