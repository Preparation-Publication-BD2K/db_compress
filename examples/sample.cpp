#include "../base.h"
#include "../model.h"
#include "../data_io.h"
#include "../categorical_model.h"
#include "../numerical_model.h"
#include "../string_model.h"
#include "../compression.h"
#include "../decompression.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sstream>

class SimpleCategoricalInterpreter: public db_compress::AttrInterpreter {
  private:
    int cap_;
  public:
    SimpleCategoricalInterpreter(int cap) : cap_(cap) {}
    bool EnumInterpretable() const { return true; }
    int EnumCap() const { return cap_; }
    int EnumInterpret(const db_compress::AttrValue* attr) const {
        return static_cast<const db_compress::EnumAttrValue*>(attr)->Value();
    }
};

const int NonFullPassStopPoint = 100;

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

    while (std::getline(fin, str)) {
        std::vector<std::string> vec;
        std::string item;
        std::stringstream sstream(str);
        while (std::getline(sstream, item, ' ')) {
            vec.push_back(item);
        }
        int type_ = type.size();
        type.push_back(type_);
        if (vec[0] == "ENUM") {
            RegisterAttrModel(type_, new db_compress::TableCategoricalCreator());
            RegisterAttrInterpreter(type_, new SimpleCategoricalInterpreter(std::stoi(vec[1])));
            err.push_back(std::stod(vec[2]));
        } else if (vec[0] == "DOUBLE") {
            RegisterAttrModel(type_, new db_compress::TableLaplaceRealCreator());
            err.push_back(std::stod(vec[1]));
        } else if (vec[0] == "INTEGER") {
            RegisterAttrModel(type_, new db_compress::TableLaplaceIntCreator());
            err.push_back(std::stod(vec[1]));
        } else if (vec[0] == "STRING") {
            RegisterAttrModel(type_, new db_compress::StringModelCreator());
            err.push_back(0);
        }
    }
    schema = db_compress::Schema(type);
    config.allowed_err = err;
    config.sort_by_attr = -1;
}

inline void AppendAttr(const std::string& str, size_t attr_type, 
                       db_compress::TupleIStream* stream,
                       std::vector<std::unique_ptr<db_compress::AttrValue> > *vec) {
    std::unique_ptr<db_compress::AttrValue> ptr;
    switch (attr_type) {
      case 0:
        ptr.reset(new db_compress::EnumAttrValue(std::stoi(str)));
        break;
      case 1:
        ptr.reset(new db_compress::IntegerAttrValue(std::stoi(str)));
        break;
      case 2:
        ptr.reset(new db_compress::DoubleAttrValue(std::stod(str)));
        break;
      case 3:
        ptr.reset(new db_compress::StringAttrValue(str));
        break;
    }
    (*stream) << ptr.get();
    vec->push_back(std::move(ptr));
}

inline std::string ExtractAttr(size_t attr_type, db_compress::TupleOStream* stream) {
    std::string ret;
    db_compress::AttrValue* attr;
    (*stream) >> attr;
    switch (attr_type) {
      case 0:
        ret = std::to_string(static_cast<const db_compress::EnumAttrValue*>(attr)->Value());
        break;
      case 1:
        ret = std::to_string(static_cast<const db_compress::IntegerAttrValue*>(attr)->Value());
        break;
      case 2:
        ret = std::to_string(static_cast<const db_compress::DoubleAttrValue*>(attr)->Value());
        break;
      case 3:
        ret = static_cast<const db_compress::StringAttrValue*>(attr)->Value();
        break;
    }
    return ret;
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
                    db_compress::TupleIStream tuple_stream(&tuple);

                    size_t count = 0;
                    std::vector< std::unique_ptr<db_compress::AttrValue> > vec;
                    while (std::getline(sstream, item, '\t')) {
                        AppendAttr(item, schema.attr_type[count++], &tuple_stream, &vec);
                    }
                    // The last item might be empty string
                    if (str[str.length() - 1] == '\t') {
                        AppendAttr("", schema.attr_type[count++], &tuple_stream, &vec);
                    }
                    if (count != schema.attr_type.size()) {
                        std::cerr << "File Format Error!\n";
                    }
                    compressor.ReadTuple(tuple);
                    tuple_cnt ++;
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
                    std::string str = ExtractAttr(schema.attr_type[i], &ostream);
                    outFile << str << (i == schema.attr_type.size() - 1 ? '\n' : '\t');
                } 
            }
        }
    }
    return 0;
}
