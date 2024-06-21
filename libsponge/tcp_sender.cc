#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _ack_no; }

void TCPSender::fill_window() {
    // 尚未syn,发送syn,TCP规定,SYN报文段不能携带数据,但要消耗掉一个序号
    if (_next_seqno == 0) {
        send_syn();
        return;
    }
    // 可以发送的最大index为 当前知道的接收方已接受的最大idx + 接收方windowsize 与 全部可发送字节 中的更小者
    auto max_sendable_idx = std::min(_ack_no + _window_size - 1,
                                     _next_seqno + stream_in().buffer_size() - 1 + fin_byte());
    if (max_sendable_idx < _next_seqno) {
        return ;
    }
    auto start = _next_seqno;
    while (start <= max_sendable_idx) {
        TCPSegment seg;
        bool is_fin_packet = stream_in().input_ended() && (start  + TCPConfig::MAX_PAYLOAD_SIZE > max_sendable_idx);
        int fin_byte = is_fin_packet ? 1 : 0;
        auto length = std::min(TCPConfig::MAX_PAYLOAD_SIZE,
                               max_sendable_idx - start + 1  - fin_byte);
        std::string data_str = stream_in().read(length);
        seg.payload() = std::move(data_str);
        seg.header().seqno = wrap(start, _isn);
        seg.header().fin = is_fin_packet;
        if (is_fin_packet) {
            _fin_sent |= is_fin_packet;
        }
        start = start + length + fin_byte;
        _segments_out.emplace(seg);
        _un_ack_buffer.emplace_back(seg);
        start_timer();
    }
    // 结束循环时（代表发包完毕）start为max_sendable_idx+1;更新_next_seqno
    _next_seqno = start;
}


//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto unwrapped_ackno = unwrap(ackno, _isn, _checkpoint);
    // 收到的报文在发送窗口外直接丢弃
    if (unwrapped_ackno > _next_seqno ) {
        return false;
    }
    if (unwrapped_ackno < _ack_no) {
        return true;
    }
    _checkpoint = unwrapped_ackno;
    // 接收方接收成功,当前被ack的索引前移
    _ack_no = unwrapped_ackno;
    _window_size = window_size;

    // remove some outstanding segments
    auto it = _un_ack_buffer.begin();
    for (; it != _un_ack_buffer.end(); it ++) {
        if (it->length_in_sequence_space() + unwrap(it->header().seqno, _isn, _checkpoint) <= unwrapped_ackno) {
            it = _un_ack_buffer.erase(it);
        }
    }

    // successful ack,更新timer相关指标
    _current_rto = _initial_retransmission_timeout;
    _con_retrans_count = 0;
    if (!_un_ack_buffer.empty()) {
        _timer = _current_rto;
    } else {
        _timer_started = false;
    }
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer_started) {
        return ;
    }
    if (_timer > ms_since_last_tick) {
        _timer -= ms_since_last_tick;
        return ;
    }
    _segments_out.emplace(_un_ack_buffer.front());
    if (_window_size > 0) {
        _con_retrans_count++;
        _current_rto*=2;
    }
    _timer = _current_rto;
}

unsigned int TCPSender::consecutive_retransmissions() const { return _con_retrans_count; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.payload() = std::string{};
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.emplace(seg);

}

void TCPSender::start_timer() {
    if (_timer_started) {
        return;
    }
    _timer_started = true;
    _timer = _current_rto;
}

int TCPSender::fin_byte() {
    return stream_in().input_ended() && !_fin_sent ? 1 :0;
}

void TCPSender::send_syn() {
    TCPSegment seg;
    seg.header().syn = true;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _un_ack_buffer.emplace_back(seg);
    _segments_out.emplace(seg);
    start_timer();
    _next_seqno++;
}

