#include "../base.h"
#include "../model.h"
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
#include <map>
#include <ctime>

class SimpleCategoricalInterpreter : public db_compress::AttrInterpreter {
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

char inputFileName[500], outputFileName[500], headerFileName[500];
bool compress;
db_compress::Schema schema;
db_compress::CompressionConfig config;
std::vector<int> attr_type;
std::vector<db_compress::EnumAttrValue> enum_vec;
std::vector<db_compress::StringAttrValue> str_vec;
std::vector<db_compress::IntegerAttrValue> int_vec;
std::vector<db_compress::DoubleAttrValue> double_vec;

std::map<std::string, int> dictionary[60];

// Read inputFileName, outputFileName and whether to
// compress or decompress. Return false if failed to recognize params.
bool ReadParameter(int argc, char **argv) {
    if (argc != 5) return false;
    if (strcmp(argv[1], "-c") == 0) compress = true;
    else if (strcmp(argv[1], "-d") == 0) compress = false;
    else return false;

    strcpy(inputFileName, argv[2]);
    strcpy(outputFileName, argv[3]);
    strcpy(headerFileName, argv[4]);
    return true;
}

int GetIndex(std::map<std::string, int>* dict, std::string value) {
    int index;
    if (dict->count(value) == 0) {
        index = dict->size();
        (*dict)[value] = index;
    }
    else index = (*dict)[value];
    return index;
}

void CreateTuple(db_compress::Tuple* tuple, const std::vector<std::string>& vec) {
    for (int i = 0; i < 2; ++i) {
        int_vec[i].Set(std::stoi(vec[i]));
        tuple->attr[i] = &int_vec[i];
    }
    str_vec[2].Set(vec[2]);
    tuple->attr[2] = &str_vec[2];
    for (int i = 3; i < 9; ++i) 
    if (i != 7) {
        int index = GetIndex(&dictionary[i], vec[i]);
        enum_vec[i].Set(index);
        tuple->attr[i] = &enum_vec[i];
    }
    // The 8th (i = 7) attribute is special, we use dummy categorical attribute to fill its position
    enum_vec[7].Set(0);
    tuple->attr[7] = &enum_vec[7];

    int cnt = 0;
    for (int i = 9; i < vec.size(); ++i) {
        enum_vec[i].Set((vec[i][0] - '0') * 10 + (vec[i][2] - '0'));
        cnt += (vec[i][0] - '0') + (vec[i][2] - '0');
        tuple->attr[i] = &enum_vec[i];
    }
    for (int i = 0; i < 20; ++i)
    if (cnt < (1 << i)) {
        enum_vec[vec.size()].Set(i);
        tuple->attr[vec.size()] = &enum_vec[vec.size()];
        break;
    }
    // Now we parse the 8th attribute
    std::string item;
    std::stringstream sstream(vec[7]);
    std::vector<std::string> fields;
    while (std::getline(sstream, item, ';')) {
        fields.push_back(item);
    }
    for (int i = 0; i < 50; ++i) {
        enum_vec[vec.size() + i * 4 + 1].Set(3);
        int_vec[vec.size() + i * 4 + 2].Set(0);
        double_vec[vec.size() + i * 4 + 3].Set(0);
        enum_vec[vec.size() + i * 4 + 4].Set(0);
        tuple->attr[vec.size() + i * 4 + 1] = &enum_vec[vec.size() + i * 4 + 1];
        tuple->attr[vec.size() + i * 4 + 2] = &int_vec[vec.size() + i * 4 + 2];
        tuple->attr[vec.size() + i * 4 + 3] = &double_vec[vec.size() + i * 4 + 3];
        tuple->attr[vec.size() + i * 4 + 4] = &enum_vec[vec.size() + i * 4 + 4];
    }
    for (int i = 0; i < fields.size(); ++i) {
        int pos = fields[i].find("=");
        std::string field_name = fields[i].substr(0, pos);
        std::string value = fields[i].substr(pos + 1);
        int index = GetIndex(&dictionary[7], field_name);
        // By default the type is integer
        int data_type = 0;
        for (int j = 0; j < value.length(); ++j)
            if (value[j] == '.' && data_type == 0) 
                data_type = 1;   // In this case we assume the data_type is float
            else if (value[j] < '0' || value[j] > '9') 
                data_type = 2;
        enum_vec[vec.size() + index * 4 + 1].Set(data_type);
        if (data_type == 0)
            int_vec[vec.size() + index * 4 + 2].Set(std::stoi(value));
        if (data_type == 1)
            double_vec[vec.size() + index * 4 + 3].Set(std::stod(value));
        if (data_type == 2)
            enum_vec[vec.size() + index * 4 + 4].Set(GetIndex(&dictionary[10 + index], value));
    }
}

void InitConfig() {
    config.sort_by_attr = 1;
    std::ifstream fin(inputFileName);
    std::ofstream fout(headerFileName);
    std::string str;

    while (std::getline(fin, str)) {
        if (str[0] == '#') {
            fout << str << "\n";
        }
        else break;
    }

    std::string item;
    std::stringstream sstream(str);
    std::vector<std::string> vec;
    while (std::getline(sstream, item, '\t')) {
        vec.push_back(item);
    }
    RegisterAttrModel(0, new db_compress::TableLaplaceIntCreator());
    RegisterAttrModel(1, new db_compress::StringModelCreator());
    RegisterAttrModel(2, new db_compress::TableCategoricalCreator());
    RegisterAttrModel(3, new db_compress::TableLaplaceRealCreator());
    RegisterAttrModel(4, new db_compress::TableCategoricalCreator());
    for (int i = 0; i < 4; ++i)
        RegisterAttrInterpreter(i, new db_compress::AttrInterpreter());
    RegisterAttrInterpreter(4, new SimpleCategoricalInterpreter(20));
    
    // First two attributes are integer attributes
    for (int i = 0; i < 2; ++i)
        attr_type.push_back(0);
    // The next attriubte is a string attribute
    attr_type.push_back(1);
    // Next six attributes are categorical attributes
    for (int i = 3; i < 9; ++i)
        attr_type.push_back(2);
    // All the rest attributes are categorical attributes
    for (int i = 9; i < vec.size(); ++i)
        attr_type.push_back(2);
    // The last attribute is the cluster index
    attr_type.push_back(4);
    // The next 200 attributes represent optional field
    for (int i = 0; i < 50; ++i) {
        attr_type.push_back(4);
        attr_type.push_back(0);
        attr_type.push_back(3);
        attr_type.push_back(2);
    }

    schema = db_compress::Schema(attr_type);
    config.allowed_err = std::vector<double>(attr_type.size(), 0);
    for (int i = attr_type.size() - 200; i < attr_type.size(); i += 4)
        config.allowed_err[i + 2] = 1e-8;
    config.skip_model_learning = true;
    config.ordered_attr_list.clear();
    // The following are manually constructed Bayesian Network.
    config.ordered_attr_list.push_back(1);
    config.ordered_attr_list.push_back(0);
    config.ordered_attr_list.push_back(vec.size());
    for (int i = 2; i < vec.size(); ++i)
        config.ordered_attr_list.push_back(i);
    for (int i = 0; i < 200; ++i)
        config.ordered_attr_list.push_back(vec.size() + i + 1);
    config.model_predictor_list.resize(attr_type.size());
    for (int i = 0; i <= vec.size() + 200; ++i)
        config.model_predictor_list[i].clear();
    for (int i = 9; i < vec.size(); ++i)
        config.model_predictor_list[i].push_back(vec.size());
    for (int i = 0; i < 50; ++i)
        for (int j = 2; j <= 4; ++j)
            config.model_predictor_list[vec.size() + i * 4 + j].push_back(vec.size() + i * 4 + 1);

    enum_vec.resize(attr_type.size());
    int_vec.resize(attr_type.size());
    str_vec.resize(attr_type.size());
    double_vec.resize(attr_type.size());
}

int main(int argc, char **argv) {
    if (!ReadParameter(argc, argv)) {
        std::cerr << "Bad Parameters.\n";
        return 1;
    }
    InitConfig();
    clock_t time = clock();
    if (compress) {
        db_compress::Compressor compressor(outputFileName, schema, config);
        while (compressor.RequireMoreIterations()) {
            if (compressor.RequireFullPass()) std::cout << "New Pass\n";
            std::ifstream fin(inputFileName);
            std::string str;                    
            int tuple_cnt = 0;

            while (std::getline(fin, str)) {
                if (str[0] == '#') continue;

                std::string item;
                std::stringstream sstream(str);
                std::vector<std::string> vec;
                while (std::getline(sstream, item, '\t')) {
                    vec.push_back(item);
                }
                db_compress::Tuple tuple(attr_type.size());
                if (++tuple_cnt % 10000 == 0) {
                    std::cout << "Processed " << tuple_cnt << " tuples\n";
                    clock_t now = clock() - time;
                    std::cout << "Elapsed " << (double)now / CLOCKS_PER_SEC << " Secs\n";
                }
                CreateTuple(&tuple, vec);
                compressor.ReadTuple(tuple);
                int times = 0;
                while (!compressor.RequireFullPass()) {
                    compressor.EndOfData();
                    compressor.ReadTuple(tuple);
                }
                //if (tuple_cnt >= 100) break;
                //if (!compressor.RequireFullPass()) break;
            }
            compressor.EndOfData();
        }
    }
    else {
        // This is for evaluating running time only!
        db_compress::Decompressor decompressor(outputFileName, schema);
        decompressor.Init();
        int tuple_cnt = 0;
        while (decompressor.HasNext()) {
            db_compress::Tuple tuple(attr_type.size());
            decompressor.ReadNextTuple(&tuple);
            if (++tuple_cnt % 10000 == 0) {
                std::cout << "Processed " << tuple_cnt << " tuples\n";
                clock_t now = clock() - time;
                std::cout << "Elapsed " << (double)now / CLOCKS_PER_SEC << " Secs\n";
            }
        }
    }
}