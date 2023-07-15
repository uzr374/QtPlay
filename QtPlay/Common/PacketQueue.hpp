#pragma once

#include <QtGlobal>
#include <condition_variable>
#include <shared_mutex>
#include <vector>

#include "../AVWrappers/Packet.hpp"

class PacketQueue final {
  Q_DISABLE_COPY_MOVE(PacketQueue);

 public:
  struct QueueState final {
    int32_t nb_packets = 0;
    int64_t size = 0, duration = 0;
    bool abort_req = false;
  };

 private:
  mutable std::shared_mutex mtx;
  QueueState state;
  std::vector<Packet> m_data;
  int read_index = 0, write_index = 0;

 public:
  PacketQueue();
  ~PacketQueue();
  inline bool isFullNolock() const { return state.nb_packets == m_data.size(); }
  inline bool isEmptyNolock() const { return state.nb_packets == 0; }
  bool put(Packet& packet);
  bool put_nullpacket(int stream_index, bool eof = false);
  bool get(Packet& dst);
  void flush();
  QueueState getState() const;
  bool isEmpty() const;
  bool isFull() const;
  void start();
  void abort();
};
