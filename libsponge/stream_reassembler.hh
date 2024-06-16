#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <memory>

class Buffer {
  public:
    std::vector<char> _data;
    size_t _current_idx;
    // list of queueing begin and end idx
    std::list<std::pair<size_t, size_t>> _queue;
    size_t _size;
    bool _eof;
    Buffer(const size_t size) :  _data(), _current_idx(0), _queue(),  _size(size), _eof(false){
        _data.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            _data.push_back(0);
        }
    }
    void processQueue();
    __attribute__((noinline))void push_substring(const std::string &data, const uint64_t index, [[maybe_unused]]const bool eof) {
        // 1.设置buffer中对应字节
        for (size_t i = 0; i < data.length(); ++i) {
            auto idx = index + i;
            if (idx >= _size) {
                break;
            }
            _data[idx] = data.at(i);
        }
        if (eof) {
            _eof = true;
        }
        // 2.将新字节序列的start-end 插入所有队列当前等待中（按start-index大小排列）
        auto it = _queue.begin();
        for (; it != _queue.end(); it ++) {
            if (it->first >= index) {
                break ;
            }
        }
        _queue.emplace(it, index, index + data.length() - 1);
        // 3.合并数组元素 todo 优化:无需遍历,理论上来说处理插入元素前后元素即可
        processQueue();
    };

    std::string genString(const size_t start, const size_t end) {
        std::string ret;
        for (size_t i = start; i <= end; ++i) {
            ret.push_back(_data.at(i));
        }
        return {ret};
    }

};


 //! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes
//    std::list<QueueElement> _queueing_elements = {};
    size_t _current_idx = 0;
    std::unique_ptr<Buffer> _buffer;
  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receives a substring and writes any newly contiguous bytes into the stream.
    //!
    //! If accepting all the data would overflow the `capacity` of this
    //! `StreamReassembler`, then only the part of the data that fits will be
    //! accepted. If the substring is only partially accepted, then the `eof`
    //! will be disregarded.
    //!
    //! \param data the string being added
    //! \param index the index of the first byte in `data`
    //! \param eof whether or not this segment ends with the end of the stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been submitted twice, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
