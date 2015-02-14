#include "attribute.h"
#include "base.h"
#include "data_io.h"
#include "compression.h"
#include "decompression.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sstream>

char inputFileName[100], outputFileName[100], configFileName[100];
bool compress;
db_compress::Schema schema;
db_compress::CompressionConfig config;

void PrintHelpInfo() {
    std::cout << "Usage:\n";
    // Todo: complete help info
}

// Read inputFileName, outputFileName, configFileName and whether to
// compress or decompress. Return false if failed to recognize params.
bool ReadParameter(int argc, char **argv) {
    if (argc != 5) return false;
    if (strcmp(argv[1], "-c") == 0) compress = true;
    else if (strcmp(argv[1], "-d") == 0) compress = false;
    else return false;
    
    strcpy(inputFileName, argv[2]);
    strcpy(outputFileName, argv[3]);
    strcpy(configFileName, argv[4]);
    return true;
}

void ReadConfig(char* configFileName) {
    std::ifstream fin(configFileName);
    std::string str;
    std::vector<int> type;
    std::vector<double> err;

    RegisterAttrValueCreator(0, new db_compress::IntegerAttrValueCreator(),
                             db_compress::BASE_TYPE_INTEGER);
    RegisterAttrValueCreator(1, new db_compress::DoubleAttrValueCreator(),
                             db_compress::BASE_TYPE_DOUBLE);
    RegisterAttrValueCreator(2, new db_compress::StringAttrValueCreator(),
                             db_compress::BASE_TYPE_STRING);
    RegisterAttrValueCreator(3, new db_compress::EnumAttrValueCreator(),
                             db_compress::BASE_TYPE_ENUM);

    while (std::getline(fin, str)) {
        std::vector<std::string> vec;
        std::string item;
        std::stringstream sstream(str);
        while (std::getline(sstream, item, ' ')) {
            vec.push_back(item);
        }
        if (vec[0] == "INTEGER") {
            type.push_back(0);
            err.push_back(std::stod(vec[1]));
        } else if (vec[0] == "DOUBLE") {
            type.push_back(1);
            err.push_back(std::stod(vec[1]));
        } else if (vec[0] == "STRING") {
            type.push_back(2);
            err.push_back(0);
        } else if (vec[0] == "ENUM") {
            type.push_back(3);
            err.push_back(std::stod(vec[2]));
        }
    }
    schema = db_compress::Schema(type);
    config.allowed_err = err;
    config.sort_by_attr = -1;
}

int main(int argc, char **argv) {
    if (argc == 1)
        PrintHelpInfo();
    else {
        if (!ReadParameter(argc, argv)) {
            std::cout << "Bad Parameters.\n";
            return 1;
        }
        ReadConfig(configFileName);
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
                    db_compress::TupleIStream tuple_stream(&tuple, schema);

                    int count = schema.attr_type.size();
                    while (std::getline(sstream, item, '\t')) {
                        tuple_stream << item;
                        count --;
                    }
                    // The last item might be empty string
                    if (str[str.length() - 1] == '\t') {
                        tuple_stream << "";
                        count --;
                    }
                    if (count != 0) {
                        std::cerr << "File Format Error!\n";
                    }
                    compressor.ReadTuple(tuple);
                    tuple_cnt ++;
                    if (tuple_cnt % 100 == 0) 
                        std::cerr << tuple_cnt << " " << "Tuples\n";
                    if (!compressor.RequireFullPass() && tuple_cnt >= 100) {
                        std::cerr << "Break\n";
                        break;
                    }
                }
                compressor.EndOfData();
                if (!compressor.RequireMoreIterations()) 
                    break;
            }
        } else {
            // Decompress
        }
    }
    return 0;
}
