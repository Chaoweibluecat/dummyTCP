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
    buffer.reserve(init_capacity);
    for (size_t i = 0; i < init_capacity; ++i) {
        buffer.push_back(0);
    }
 }

size_t ByteStream::write(const string &data) {
    size_t length = std::min(remaining_capacity(), data.length());
    if (length == 0) {
        return 0;
    }
    for (size_t i = 0; i < length; ++i) {
        buffer[(_current_write_idx + i) % capacity] = data[i];
    }
    _current_write_idx = (_current_write_idx + length) % capacity;

    if (_current_write_idx == _current_read_idx) {
        _buffer_size = capacity;
    } else {
        _buffer_size = _current_write_idx >= _current_read_idx
                           ? _current_write_idx - _current_read_idx
                           : _current_write_idx - _current_read_idx + capacity;
    }

    _bytes_written += length;
    return length;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string res;
    if (len <= 0) {
        return res;
    }
    auto peek_length = std::min(buffer_size(), len);
    for (size_t i = 0; i < peek_length ; ++i) {
        res.push_back(buffer.at((i + _current_read_idx) % capacity));
    }
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    string res;
    auto length = std::min(buffer_size(), len);
    for (size_t i = 0; i < length ; ++i) {
        res.push_back(buffer.at((i + _current_read_idx) % capacity));
    }
    _current_read_idx = (_current_read_idx + length) % capacity;
    // update buffer_size;
    // 理论上来说,_current_write_idx - _current_read_idx就可以得到buffersize就可以得到buffersize;不需要记录这个字段
    // 但是实际上_current_write_idx == _current_read_idx可以同时代表所有字节都可读，（write超了一圈） 或者所有都不可读（read已追上）
    // 所有此时多用一个字段存下
    if (_current_write_idx == _current_read_idx) {
        _buffer_size = 0;
    } else {
        _buffer_size = _current_write_idx >= _current_read_idx
                           ? _current_write_idx - _current_read_idx
                           : _current_write_idx - _current_read_idx + capacity;
    }

    _bytes_read += len;
}

void ByteStream::end_input() {end = true;}

bool ByteStream::input_ended() const { return end; }

size_t ByteStream::buffer_size() const {
    return _buffer_size;}

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return buffer_empty() && end; }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return capacity - buffer_size(); }
