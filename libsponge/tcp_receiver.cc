#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    // delayed syn
    if (seg.header().syn && _initial_seqno.has_value()) {
        return false;
    }
    // 1.set initial syn number if necessary
    if (seg.header().syn && !_initial_seqno.has_value()) {
        _initial_seqno = std::make_optional<WrappingInt32>(seg.header().seqno);
        _checkpoint = std::make_optional<uint64_t>(0);
    }
    // 如果收到fin,且fin前置报文齐全，也没有在等的，（对应input_ended）返回false
    if (stream_out().input_ended()) {
        return false;
    }
    if (!_initial_seqno.has_value()) {
        return false;
    }
    auto real_start = unwrap(seg.header().seqno, _initial_seqno.value(), _checkpoint.value());
    // 非sync报文,end_idx 小于当前窗口位置 或 start_idx>当前windowsize能承受的最大,丢弃
    if (!seg.header().syn && real_start + seg.length_in_sequence_space() <= stream_out().bytes_written() + 1) {
        return false;
    }
    if (real_start >= real_ackno() + _reassembler.stream_out().remaining_capacity()) {
        return false;
    }

    _checkpoint = std::make_optional<uint64_t>(real_start);
    // 如果不是syn报文,后续报文插入的idx需要-1 （代表seqno 中syn的并不真正在reassemble中占位）
    auto push_idx = seg.header().syn ? real_start : real_start - 1;
    _reassembler.push_substring(seg.payload().copy(), push_idx, seg.header().fin);
    return true;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_initial_seqno.has_value()) {
        return {};
    }
    return {wrap(real_ackno(), _initial_seqno.value())};
}

size_t TCPReceiver::window_size() const {
   return _reassembler.stream_out().remaining_capacity();
//    return real_size == 0 ? 1 : real_size;
}

// bytes written +1 = 下一个数据号;如果有fin还要再加一(fin对应input_ended而不是eof,byte_stream的eof是对读者的api）
uint64_t TCPReceiver::real_ackno() const{
    return stream_out().bytes_written() + 1 + (stream_out().input_ended() ? 1: 0);
}

