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
    if (seg.header().rst) {
        handle_rst(seg);
        return ;
    }

    // listen状态（对应_sender.next_seqno_absolute() == 0)
    if (_sender.next_seqno_absolute() == 0) {
        // 收到ack直接rst,收到其他非syn报文直接drop
        if (seg.header().ack) {
            send_rst({});
            return ;
        } else if (!seg.header().syn) {
            return;
        }
    }
    // syn_sent,对应 _sender.next_seqno_absolute() == _sender.bytes_in_flight() == 1
    else if (_sender.next_seqno_absolute() == _sender.bytes_in_flight() ) {
        // syn_sent + 错误的ackno -> rst
        if (seg.header().ack && !_sender.valid_syn_sent_ackno(seg.header().ackno)) {
            send_rst(seg.header().ackno);
            return;
        }
        if (seg.header().ack && seg.payload().str().size() > 0 ) {
            return ;
        }
        //  peer没有发过syn时,忽略peer的ack @see fsm_connect Test1
        if (!_receiver.ackno().has_value() && !seg.header().syn && seg.header().ack) {
            return;
        }
    }
    //active_close case2:closing状态,支持两边同时关闭,这里的对方ack可以直接ignore
    // (因为正常走到_receiver.segment_received(seg)会因为stream已关闭返回false,而走到发一个ack的逻辑里)
    auto in_closing = TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
                      TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_SENT;

    _last_time_seg_received = 0;
    // 根据规范,sender收到不合理的ackno或者receiver收到不合理的seqno,都是发一个ack
    auto valid_seq_no = in_closing || _receiver.segment_received(seg) ;
    // 被动关闭 _receiver.stream_out().input_ended()代表FIN_RECV状态（见tcpStateSummary)
    if (_receiver.stream_out().input_ended() &&
        (!_sender.stream_in().buffer_empty() || !_sender.stream_in().input_ended())) {
            _linger_after_streams_finish = false;
        }
    bool valid_seg = false;
    if (seg.header().ack) {
        valid_seg = _sender.ack_received(seg.header().ackno, seg.header().win);
    }


    if (seg.length_in_sequence_space() > 0 || !valid_seg || !valid_seq_no) {
        // 对方报文占序列号（是syn/fin/或带数据了，必须至少发回一个ack）
        fill_window_and_send(true);
    } else {
        // 对方报文不占序列号（说明是纯净ack,那么此时如果本地也没数据可发,可以直接装死）
        // 对方的空ack无需回复ack,否则就是无限的ack pingpong
        fill_window_and_send(false);
    }
}

bool TCPConnection::active() const {
    // check error
    if (_sender.stream_in().error() || _receiver.stream_out().error()) {
        return false;
    }
    // 非linger状态,确保输入全接受、输出全输出、对面全ackno
    if (!_linger_after_streams_finish) {
        return !_sender.stream_in().input_ended()
               || !_sender.stream_in().buffer_empty()
               || !_receiver.stream_out().input_ended()
               || bytes_in_flight() != 0;
    }
    // 否则根据lingering time判断
    return _last_time_seg_received < 10 * _cfg.rt_timeout;
}

size_t TCPConnection::write(const string &data) {
    if (data.empty()) {
        return 0;
    }
    auto res = _sender.stream_in().write(data);
    fill_window_and_send(false);
    return res;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _last_time_seg_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.segments_out().empty()) {
        return;
    }
    if (_sender.consecutive_retransmissions() >= TCPConfig::MAX_RETX_ATTEMPTS) {
        send_rst({});
        return;
    }
    enrich_outs_and_send();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    fill_window_and_send(false);
    // 入栈流先于出栈流eof,那么linger = false（代表passive close）
    if (_receiver.stream_out().input_ended()) {
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::connect() {
    fill_window_and_send(false);
}


void TCPConnection::fill_window_and_send(bool send_empty_ack) {
    //填满窗口,sender没东西发那根据入参决定是否需要发个纯净ack
    _sender.fill_window();
    if (_sender.segments_out().empty() && send_empty_ack) {
        _sender.send_empty_segment();
    }
    enrich_outs_and_send();
}

/**
 * 发送rst报文;如果不指定seqno,则为sender生成的seqno
 * (syn_sent状态下,收到badack会使用收到的非法的ackno作为rst报文的seqno,神奇）
 * @param seqno
 */
void TCPConnection::send_rst(std::optional<WrappingInt32> seqno) {
    _sender.send_empty_segment();
    auto front = _sender.segments_out().front();
    front.header().rst = true;
    if (seqno.has_value()) {
        front.header().seqno = seqno.value();
    }
    _segments_out.emplace(front);
    // 设置下error
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}


void TCPConnection::enrich_outs_and_send() {
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

/**
 * syn_sent + (ack+rst) = reset
 * syn_sent + (rst) = syn_sent
 * established + legal_seqno_rst = reset
 * @param seg
 */
void TCPConnection::handle_rst(const TCPSegment &seg) {
    if (
        // 连接未建立时，（syn_sent状态下,只承认ack+rst,裸rst不认)
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_SENT&&
        !seg.header().ack) {
        return;
    }
    if (seg.header().ack && !_sender.valid_syn_sent_ackno(seg.header().ackno)) {
        return ;
    }
    // 其余状态,响应seqno合法的rst
    if (_receiver.segment_received(seg)) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
    }
}
