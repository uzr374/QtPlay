#include "PacketQueue.hpp"

PacketQueue::PacketQueue() {
  m_data.resize(1500);
  abort();
}

PacketQueue::~PacketQueue() { }

void PacketQueue::start() {
  std::scoped_lock lck(mtx);
  state.abort_req = false;
}

void PacketQueue::abort() {
  std::scoped_lock lck(mtx);
  state.abort_req = true;
}

void PacketQueue::flush() {
  std::scoped_lock lck(mtx);
  state = QueueState();
  read_index = 0;
  write_index = 0;
  for (auto& pkt : m_data) pkt.clear();
}

/* Takes ownership of and resets Packet's underlying AVPacket */
bool PacketQueue::put(Packet& packet) {
  std::scoped_lock lck(mtx);
  if (state.abort_req || isFullNolock()) return false;
  auto& dst = m_data[write_index];
  Packet::copyParams(packet, dst);
  av_packet_move_ref(dst.avData(), packet.avData());
  ++state.nb_packets;
  state.size += dst.sizePlusSizeof();
  state.duration += dst.duration();
  ++write_index;
  if (write_index == m_data.size()) write_index = 0;

  return true;
}

bool PacketQueue::put_nullpacket(int stream_index, bool eof) {
  if (isFull()) throw;
  if (stream_index < 0) return false;
  Packet pkt;
  pkt.setFlush(true);
  pkt.setEOF(eof);
  pkt.avData()->size = 0;
  pkt.avData()->data = nullptr;
  pkt.avData()->stream_index = stream_index;
  return put(pkt);
}

bool PacketQueue::get(Packet& dst) {
  dst.clear();
  std::unique_lock lck(mtx);
  const auto not_empty = !state.abort_req && !isEmptyNolock();
  if (not_empty) {
    auto& pkt = m_data[read_index];
    state.size -= pkt.sizePlusSizeof();
    state.duration -= pkt.duration();
    --state.nb_packets;
    Packet::copyParams(pkt, dst);
    av_packet_move_ref(dst.avData(), pkt.avData());
    pkt.clear();
    ++read_index;
    if (read_index == m_data.size()) read_index = 0;
  }

  return not_empty;
}

PacketQueue::QueueState PacketQueue::getState() const {
  std::shared_lock lck(mtx);
  return state;
}

bool PacketQueue::isEmpty() const {
  std::shared_lock lck(mtx);
  return isEmptyNolock();
}

bool PacketQueue::isFull() const {
  std::shared_lock lck(mtx);
  return isFullNolock();
}
