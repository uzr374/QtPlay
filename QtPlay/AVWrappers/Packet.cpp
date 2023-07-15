#include "Packet.hpp"

#include <utility>

void Packet::copyParams(const Packet& src, Packet& dst) {
  dst.is_flush = src.is_flush;
  dst.is_eof = src.is_eof;
}

void Packet::clear() {
  av_packet_unref(m_pkt);
  is_flush = is_eof = false;
}

bool Packet::isFlush() const { return is_flush; }

bool Packet::isEOF() const { return is_eof; }

int Packet::size() const { return m_pkt->size; }

int64_t Packet::duration() const { return m_pkt->duration; }

int Packet::sizePlusSizeof() const { return size() + sizeof(decltype(*this)); }

AVPacket* Packet::avData() { return m_pkt; }

const AVPacket* Packet::constAvData() const { return m_pkt; }

void Packet::setFlush(bool flush) { is_flush = flush; }

void Packet::setEOF(bool eof) { is_eof = eof; }

int Packet::streamIndex() const { return m_pkt->stream_index; }

int64_t Packet::bytePos() const { return m_pkt->pos; }

Packet::Packet() : m_pkt(av_packet_alloc()) {}

Packet::Packet(const Packet& rhs) : Packet() {
  copyParams(rhs, *this);
  av_packet_ref(avData(), rhs.m_pkt);
}

Packet::Packet(Packet&& rhs) noexcept {
  copyParams(rhs, *this);
  m_pkt = rhs.m_pkt;
  rhs.m_pkt = nullptr;
}

Packet::~Packet() {
  if (m_pkt) {
    av_packet_free(&m_pkt);
  }
}

Packet& Packet::operator=(const Packet& rhs) {
  av_packet_unref(m_pkt);
  copyParams(rhs, *this);
  av_packet_ref(avData(), rhs.m_pkt);

  return *this;
}

Packet& Packet::operator=(Packet&& rhs) noexcept {
  clear();
  copyParams(rhs, *this);
  av_packet_move_ref(m_pkt, rhs.avData());

  return *this;
}
