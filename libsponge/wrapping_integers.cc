#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // todo uint64溢出?需要考虑吗,疑似不需要
    return WrappingInt32{ static_cast<uint32_t>(n + isn.raw_value())};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `_checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    auto check_seq = wrap(checkpoint, isn);
    uint64_t res;
    // diff代表了最小差距,正的代表可以正向走比较进,负的代表“最好”往checkpoint往负方向走
    // 例如N=0,Wrap（_checkpoint） = 2^32-1, 则diff = 1;最好向checkpoint正方向走一byte
    // 例如N= 2^32-1,Wrap（_checkpoint） = 0, 则diff = -1;最好向走checkpoint负方向走byte
    // 类似地N=0,Wrap（_checkpoint） = 1; diff = -1;最好向checkpoint负方向走一byte
    // 类似地N=1,Wrap（_checkpoint） = 0; diff = 1 最好向checkpoint正方向走一byte
    // 因为checkpoint不能为负，所以diff为负但是绝对值超过checkpoint时,还是需要正向走，距离为(1l << 32) + diff
    auto diff = n - check_seq;
    res = diff + static_cast<int64_t>(checkpoint) >= 0 ? diff + checkpoint : (1l << 32) + diff + checkpoint;
    return res ;
}
