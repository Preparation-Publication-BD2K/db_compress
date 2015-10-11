#include "corel.h"
#include "../data_io.h"
#include "../numerical_model.h"
#include "../compression.h"
#include "../decompression.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sstream>

const int NonFullPassStopPoint = 2000;
const double ErrThreshold = 0.01;

char inputFileName[100], outputFileName[100];
bool compress;
db_compress::Schema schema;
db_compress::CompressionConfig config;

// Read inputFileName, outputFileName, configFileName and whether to
// compress or decompress. Return false if failed to recognize params.
bool ReadParameter(int argc, char **argv) {
    if (argc != 4) return false;
    if (strcmp(argv[1], "-c") == 0) compress = true;
    else if (strcmp(argv[1], "-d") == 0) compress = false;
    else return false;
    
    strcpy(inputFileName, argv[2]);
    strcpy(outputFileName, argv[3]);
    return true;
}

void SetConfig() {
    RegisterAttrModel(0, new ColorModelCreator());
    RegisterAttrModel(1, new db_compress::TableLaplaceIntCreator());
    RegisterAttrInterpreter(0, new ColorInterpreter());
    RegisterAttrInterpreter(1, new db_compress::AttrInterpreter());
    std::vector<int> type(33, 0); type[0] = 1;
    std::vector<double> err(33, ErrThreshold);
    schema = db_compress::Schema(type);
    config.allowed_err = err;
    config.sort_by_attr = 0;
}

inline void AppendAttr(double attr, db_compress::TupleIStream* stream,
                       std::vector<std::unique_ptr<ColorAttr> > *vec) {
    std::unique_ptr<ColorAttr> ptr;
    ptr.reset(new ColorAttr(attr));
    (*stream) << ptr.get();
    vec->push_back(std::move(ptr));
}

inline double ExtractAttr(size_t attr_type, db_compress::TupleOStream* stream) {
    db_compress::AttrValue* attr;
    (*stream) >> attr;
    if (attr_type == 0)
        return static_cast<const ColorAttr*>(attr)->Value();
    else
        return static_cast<const db_compress::IntegerAttrValue*>(attr)->Value();
}

int main(int argc, char **argv) {
    if (!ReadParameter(argc, argv)) {
        std::cout << "Bad Parameters.\n";
        return 1;
    }
    SetConfig();
    if (compress) {
        // Compress
        db_compress::Compressor compressor(outputFileName, schema, config);
        int iter_cnt = 0;
        while (1) {
            std::cout << "Iteration " << ++iter_cnt << " Starts\n";
            std::ifstream inFile(inputFileName);
            std::string str;
            int tuple_cnt = 0;
            while (std::getline(inFile,str)) {
                std::stringstream sstream(str);
                std::string item;
                db_compress::Tuple tuple(schema.attr_type.size());
                db_compress::TupleIStream tuple_stream(&tuple);
                
                db_compress::IntegerAttrValue attr(++tuple_cnt);
                tuple_stream << &attr; 

                size_t count = 0;
                std::vector< std::unique_ptr<ColorAttr> > vec;
                while (std::getline(sstream, item, ' ')) {
                    if (++ count > 1)
                        AppendAttr(std::stod(item), &tuple_stream, &vec);
                }
                // The first item is tuple id
                if (count != schema.attr_type.size()) {
                    std::cerr << "File Format Error!\n";
                }
                compressor.ReadTuple(tuple);
                if (!compressor.RequireFullPass() && 
                    tuple_cnt >= NonFullPassStopPoint) {
                    break;
                }
            }
            compressor.EndOfData();
            if (!compressor.RequireMoreIterations()) 
                break;
        }
    } else {
        // Decompress
        db_compress::Decompressor decompressor(inputFileName, schema);
        std::ofstream outFile(outputFileName);
        decompressor.Init();
        while (decompressor.HasNext()) {
            db_compress::ResultTuple tuple;
            decompressor.ReadNextTuple(&tuple);
            db_compress::TupleOStream ostream(tuple);
            for (size_t i = 0; i < schema.attr_type.size(); ++i) {
                double attr = ExtractAttr(schema.attr_type[i], &ostream);
                outFile << attr << (i == schema.attr_type.size() - 1 ? '\n' : ' ');
            } 
        }
    }
    return 0;
}
