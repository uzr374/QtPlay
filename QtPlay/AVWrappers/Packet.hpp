#pragma once

extern "C" {
#include <libavcodec/packet.h>
}

#pragma comment(lib, "avcodec.lib")

class Packet final {
 private:
  AVPacket* m_pkt = nullptr;
  bool is_flush = false, is_eof = false;

 public:
  Packet();
  Packet(const Packet&);
  Packet(Packet&&) noexcept;
  ~Packet();

  Packet& operator=(const Packet&);
  Packet& operator=(Packet&&) noexcept;

  void clear();
  AVPacket* avData();
  const AVPacket* constAvData() const;
  bool isFlush() const;
  bool isEOF() const;
  int size() const;
  int64_t duration() const;
  int sizePlusSizeof() const;
  void setFlush(bool flush = true);
  void setEOF(bool eof = true);
  int streamIndex() const;
  int64_t bytePos() const;

  static void copyParams(const Packet& src, Packet& dst);
};
