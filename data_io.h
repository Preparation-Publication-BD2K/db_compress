#ifndef DATA_IO_H
#define DATA_IO_H

#include "attribute.h"
#include "base.h"
#include <vector>
#include <iostream>
#include <fstream>

namespace db_compress {

class TupleIStream {
  private:
    Tuple* tuple_;
    const Schema& schema_;
    int index_;
  public:
    TupleIStream(Tuple* tuple, const Schema& schema);
    template <typename T>
    friend TupleIStream& operator<<(TupleIStream& stream, T val);
};

template<typename T>
TupleIStream& operator<<(TupleIStream& tuple_stream, T val) {
    size_t attr_type = tuple_stream.schema_.attr_type[tuple_stream.index_];
    AttrValueCreator* creator = GetAttrValueCreator(attr_type);
    AttrValue* attr = creator->GetAttrValue(val);

    if (attr == NULL) {
        std::cerr << "Error while reading tuple attr\n";
    }
    tuple_stream.tuple_->attr[tuple_stream.index_ ++].reset(attr);
    return tuple_stream;
}

template TupleIStream& operator<<(TupleIStream&, int);
template TupleIStream& operator<<(TupleIStream&, double);
template TupleIStream& operator<<(TupleIStream&, const std::string&);
template TupleIStream& operator<<(TupleIStream&, size_t);

class TupleOStream {
  private:
    const Tuple& tuple_;
    const Schema& schema_;
    int index_;
  public:
    TupleOStream(const Tuple& tuple, const Schema& schema);
    template <typename T>
    friend TupleOStream& operator>>(TupleOStream& stream, T& val);
};

template <typename T>
TupleOStream& operator>>(TupleOStream& tuple_stream, T& val) {
    int attr_type = tuple_stream.schema_.attr_type[tuple_stream.index_];
    AttrValueCreator* creator = GetAttrValueCreator(attr_type);
    creator->ReadAttrValue(*tuple_stream.tuple_.attr[tuple_stream.index_ ++], &val);
    return tuple_stream;
}

template TupleOStream& operator>>(TupleOStream&, int&);
template TupleOStream& operator>>(TupleOStream&, double&);
template TupleOStream& operator>>(TupleOStream&, std::string&);
template TupleOStream& operator>>(TupleOStream&, size_t&);

/*
 * ByteWriter is a utility class that can be used to write bit strings.
 * The constructor takes a vector of integers indicating the (predetermined)
 * block lengths. Then the class provide interface to continue writing bit 
 * strings to any of these blocks in arbitrary order. Note that only if the
 * object is destoryed will the data be completely written into the file, 
 * otherwise some of the data might be held in memory buffer.
 */
class ByteWriter {
  private:
    // Since we can only write byte as a unit, sometimes we have unwritten
    // prefix/suffix bis string that is shorter than 8 bits, we store them
    // in the buffer.
    std::vector<unsigned char> block_unwritten_prefix_;
    std::vector<unsigned char> block_unwritten_suffix_;
    // The starting point of incoming bit stream for each block
    std::vector<size_t> block_pos_;
    std::ofstream file_;
  public:
    ByteWriter(std::vector<size_t>* block_length, const std::string& file_name);
    ~ByteWriter();
    void WriteByte(unsigned char byte, size_t block);
    // Only write the least significant (len) bits
    void WriteLess(unsigned char byte, size_t len, size_t block);
    // Write 16 bits at once
    void Write16Bit(unsigned int val, size_t block);
    // Write 32 bits at once
    void Write32Bit(unsigned char byte[4], size_t block);
};

/* 
 * ByteReader is a utility class that can be used to read bit strings.
 */
class ByteReader {
  private:
    std::ifstream fin_;
    unsigned int buffer_, buffer_len_;
  public:
    ByteReader(const std::string& file_name);
    ~ByteReader();
    unsigned char ReadByte();
    bool ReadBit();
    unsigned int Read16Bit();
    void Read32Bit(unsigned char* bytes);
};

}  // namespace db_compress

#endif
