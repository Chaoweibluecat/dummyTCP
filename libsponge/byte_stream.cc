#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <vector>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t init_capacity) : capacity(init_capacity) {
 }

size_t ByteStream::write(const string &data) {
    size_t length = capacity - buffer_size() > data.length() ? data.length() : capacity - buffer_size();
    for (size_t i = 0; i < length; ++i) {
        buffer.push_back(data[i]);
    }
    _bytes_written += length;
    return length;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string res;
    size_t it = len > buffer_size() ? buffer_size() : len;
    for (size_t i = 0; i < it ; ++i) {
        res.push_back(buffer.at(i));
    }
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    string res = peek_output(len);
    buffer.erase(buffer.begin(), buffer.begin() + len);
    _bytes_read += len;
}

void ByteStream::end_input() {end = true;}

bool ByteStream::input_ended() const { return end; }

size_t ByteStream::buffer_size() const { return buffer.size(); }

bool ByteStream::buffer_empty() const { return buffer.empty(); }

bool ByteStream::eof() const { return buffer_empty() && end; }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return capacity - buffer_size(); }
