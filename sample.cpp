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
    if (argc != 4) return false;
    if (strcmp(argv[1], "-c") == 0) compress = true;
    else if (strcmp(argv[1], "-d") == 0) compress = false;
    else return false;
    
    strcpy(inputFileName, argv[2]);
    strcpy(outputFileName, argv[3]);
    strcpy(configFileName, argv[4]);
    return true;
}

void ReadConfig(char* configFileName) {
}

int main(int argc, char **argv) {
    if (argc == 0)
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
            while (1) {
                std::ifstream inFile(inputFileName);
                std::string str;
                while (std::getline(inFile,str)) {
                    std::stringstream sstream(str);
                    std::string item;
                    db_compress::Tuple tuple(schema.attr_type.size());
                    db_compress::TupleIStream tuple_stream(&tuple, schema);
                    while (std::getline(sstream, item, ',')) {
                        tuple_stream << item;
                    }
                    compressor.ReadTuple(tuple);
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
