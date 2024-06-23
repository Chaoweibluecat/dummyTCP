#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().buffer_size(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _last_time_seg_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // _sender已发送syn,但是peer没有发过syn时,忽略peer的ack @see fsm_connect Test1
    if (!_receiver.ackno().has_value() && !seg.header().syn) {
        return ;
    }
    // 根据规范,sender收到不合理的ackno或者receiver收到不合理的seqno,都是发一个ack
    if (seg.length_in_sequence_space() > 0 && !_receiver.segment_received(seg)) {
        // unacceptable packet,简单返回一个ackno和window的报文
        fill_window_and_send(true);
    };
    _last_time_seg_received = 0;
    _sender.ack_received(seg.header().ackno, seg.header().win);
    if (seg.length_in_sequence_space() > 0) {
        // 对方报文占序列号（是syn/fin/或带数据了，必须至少发回一个ack）
        fill_window_and_send(true);
    } else {
        // 对方报文不占序列号（说明是纯净ack,那么此时如果本地也没数据可发,可以直接装死）
        // 对方的空ack无需回复ack,否则就是无限的ack pingpong
        fill_window_and_send(false);
    }
}

bool TCPConnection::active() const {
    return !_sender.stream_in().input_ended();
//    return TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV
//                                     && TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED;
}

size_t TCPConnection::write(const string &data) {
    return _sender.stream_in().write(data);
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _last_time_seg_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
}

void TCPConnection::connect() {
    fill_window_and_send(false);
}


void TCPConnection::fill_window_and_send(bool send_empty_ack) {
    //填满窗口,sender没东西发那就发个ack
    _sender.fill_window();
    if (_sender.segments_out().empty() && send_empty_ack) {
        _sender.send_empty_segment();
    }
    while (!_sender.segments_out().empty()) {
        auto seg = _sender.segments_out().front();
        if (_receiver.ackno().has_value()) {
            seg.header().ackno = _receiver.ackno().value();
            seg.header().ack = true;
        }
        seg.header().win = _receiver.window_size();
        _segments_out.emplace(seg);
        _sender.segments_out().pop();
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
