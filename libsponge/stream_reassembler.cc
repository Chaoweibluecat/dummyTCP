#include "stream_reassembler.hh"
#include "iostream"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity),
    _capacity(capacity), _buffer(std::make_unique<ReassembleBuffer>(_capacity)){}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring([[maybe_unused]]const std::string &data,
                                       [[maybe_unused]]const uint64_t index,
                                       [[maybe_unused]]const bool eof) {
    if (!data.empty()) {
        size_t end_idx = index + data.length() - 1;
        // 找到目前能接受的最大字节; 大于此字节的无法放到output中
        size_t actual_end = _max_acceptable_idx() > end_idx ? end_idx : _max_acceptable_idx();
        //todo 优化拷贝
        auto push_str = data.substr(0, actual_end - index + 1);
        if (!push_str.empty()) {
            _buffer->push_substring(push_str, index, eof);
            // 可用buffer向前移动了,代表output stream又可以有新输入
            if (_current_idx < _buffer->_current_idx) {
                _output.write(_buffer->genString(_current_idx, _buffer->_current_idx - 1));
                _current_idx = _buffer->_current_idx;
            }
        }
    } else {
        _buffer->_eof = _buffer->_eof || eof;
    }
    // 如果buffer已经接收到eof,且剩余待处理队列为空,说明可以标记输出数组没有新输入的
    if (_buffer->_eof && empty()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t res = 0;
    for (const auto &item : _buffer->_queue) {
        res += item.second - item.first + 1;
    }
    return res;
}

bool StreamReassembler::empty() const { return _buffer->_queue.empty(); }

size_t StreamReassembler::_max_acceptable_idx() {
    return _current_idx + _output.remaining_capacity() - 1;
}


void ReassembleBuffer::processQueue() {
    for (auto it = _queue.begin(); it != _queue.end();) {
        // cover prior to overlap or contigous
        // (it->second >= std::next(it)->second 成立时，it->second >= std::next(it)->first - 1一定成立——
        if (std::next(it) != _queue.end() && it->second >= std::next(it)->second) {
            // first one cover second one
            it = _queue.erase(std::next(it));
            it = std::prev(it);
        }
        // string overlap or contigous, we do merge
        else if (std::next(it) != _queue.end() && it->second >= std::next(it)->first - 1) {
            auto start = it->first;
            it = _queue.erase(it);
            it->first = start;
        }
        // second one covers first one(两个包起点一样时可能发生）
        else if (std::next(it) != _queue.end()
                   && it->first >= std::next(it)->first
                   && it->second <= std::next(it)->second) {
            it = _queue.erase(it);
        }
        else {
            it++;
        }
    }
        // 队首出队
        auto first_element = _queue.begin();
        //判断队首的start_idx是否小于等于当前字节流的index，是的话说明队首可以出队,
        if (first_element != _queue.end() && first_element->first <= _current_idx) {
        // 这里需要判断队首元素能有效更新已读字节idx
        // 如果currentIdx此时为1001（对应已读 0-1000），区分队首元素是 900-1000（无效） / 900-1001（有效）
        auto head_element_end_idx = first_element->second;
            _current_idx = _current_idx > head_element_end_idx ?  _current_idx : head_element_end_idx +1;
            _queue.pop_front();
        }
    }

    void ReassembleBuffer::push_substring(const std::string &data, const uint64_t index, const bool eof) {
        auto end_idx = index + data.length() - 1;
        // 1.设置buffer中对应字节
        for (size_t i = index; i <= end_idx; ++i) {
            // 某个特定idx, 输入中的坐标为idx-start_idx; 预计写入的位置为idx % size
            _data[i % _size] = data.at(i - index);
        }
        _eof |= eof;
        // 2.将新字节序列的start-end 插入所有队列当前等待中（按start-index大小排列）
        auto it = _queue.begin();
        for (; it != _queue.end(); it ++) {
            if (it->first >= index) {
            break ;
            }
        }
        _queue.emplace(it, index, end_idx);
        // 3.合并数组元素 todo 优化:无需遍历,理论上来说处理插入元素前后元素即可
        processQueue();
    };


