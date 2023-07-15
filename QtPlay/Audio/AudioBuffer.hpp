#pragma once

#include <QtGlobal>
#include <algorithm>
#include <atomic>
#include <vector>

//! \brief Ringbuffer (aka circular buffer) data structure for use in concurrent
//! audio scenarios.
//!
//! Other than minor modifications, this ringbuffer is a copy of Tim Blechmann's
//! fine work, found as the base structure of boost::lockfree::spsc_queue
//! (ringbuffer_base). Whereas the boost::lockfree data structures are meant for
//! a wide range of applications / archs, this version specifically caters to
//! audio processing.
//!
//! The implementation remains lock-free and thread-safe within a single write
//! thread / single read thread context.
//!
//! \note \a T must be POD.
//!
//!

class AudioRingBufferF {
  Q_DISABLE_COPY_MOVE(AudioRingBufferF);
  using size_type = std::uint32_t;
  using sample_t = std::float_t;

 public:
  //! Constructs a RingBufferT with size = 0
  AudioRingBufferF() {}

  ~AudioRingBufferF() {}

  //! Resizes the container to contain \a count maximum elements. Invalidates
  //! the internal buffer and resets read / write indices to 0. \note Must be
  //! synchronized with both read and write threads.
  void resize(size_type count) noexcept {
    const auto allocatedSize =
        count + 1;  // one bin is used to distinguish between the read and write
                    // indices when full.
    mDataStore.resize(allocatedSize, sample_t());
    mData = mDataStore.data();
    mAllocatedSize = allocatedSize;
    clear();
  }

  //! Invalidates the internal buffer and resets read / write indices to 0.
  //! \note Must be synchronized with both read and write threads.
  void clear() {
    mWriteIndex = 0;
    mReadIndex = 0;
  }

  //! Returns the maximum number of elements.
  size_type getSize() const noexcept { return mAllocatedSize - 1; }

  //! Returns the number of elements available for writing. \note Only safe to
  //! call from the write thread.
  size_type getAvailableWrite() const noexcept {
    return getAvailableWrite(mWriteIndex.load(), mReadIndex.load());
  }

  //! Returns the number of elements available for reading. \note Only safe to
  //! call from the read thread.
  size_type getAvailableRead() const noexcept {
    return getAvailableRead(mWriteIndex.load(), mReadIndex.load());
  }

  // Only safe to call from the writer thread
  // Reader thread should call getAvaliableRead() to get amount of the buffered
  // elements
  size_type getBufferedElems_Writer() const noexcept {
    return getSize() - getAvailableWrite();
  }

  //! \brief Writes \a count elements into the internal buffer from \a array.
  //! \return `true` if all elements were successfully written, or `false`
  //! otherwise.
  //!
  //! \note only safe to call from the write thread.
  //! TODO: consider renaming this to writeAll / readAll, and having generic
  //! read / write that just does as much as it can
  bool write(const sample_t* array, size_type count) noexcept {
    const auto writeIndex = mWriteIndex.load(std::memory_order_relaxed);
    const auto readIndex = mReadIndex.load(std::memory_order_acquire);

    if (count > getAvailableWrite(writeIndex, readIndex)) return false;

    auto writeIndexAfter = writeIndex + count;

    if (writeIndex + count > mAllocatedSize) {
      const auto countA = mAllocatedSize - writeIndex;
      const auto countB = count - countA;

      std::copy(array, array + countA, mData + writeIndex);
      std::copy(array + countA, array + countA + countB, mData);

      writeIndexAfter -= mAllocatedSize;
    } else {
      std::copy(array, array + count, mData + writeIndex);
      if (writeIndexAfter == mAllocatedSize) writeIndexAfter = 0;
    }

    mWriteIndex.store(writeIndexAfter, std::memory_order_release);

    return true;
  }

  //! \brief Reads \a count elements from the internal buffer into \a array.
  //! \return `true` if all elements were successfully read, or `false`
  //! otherwise.
  //!
  //! \note only safe to call from the read thread.
  //! \note If array is null, then elements are just being skipped
  bool read(sample_t* array, size_type count) noexcept {
    const bool drop = (array == nullptr);

    const auto writeIndex = mWriteIndex.load(std::memory_order_acquire);
    const auto readIndex = mReadIndex.load(std::memory_order_relaxed);

    if (count > getAvailableRead(writeIndex, readIndex)) return false;

    auto readIndexAfter = readIndex + count;

    if (readIndex + count > mAllocatedSize) {
      const auto countA = mAllocatedSize - readIndex;
      const auto countB = count - countA;

      if (!drop) {
        std::copy(mData + readIndex, mData + readIndex + countA, array);
        std::copy(mData, mData + countB, array + countA);
      }

      readIndexAfter -= mAllocatedSize;
    } else {
      if (!drop) {
        std::copy(mData + readIndex, mData + readIndex + count, array);
      }

      if (readIndexAfter == mAllocatedSize) readIndexAfter = 0;
    }

    mReadIndex.store(readIndexAfter, std::memory_order_release);

    return true;
  }

  /* Note: only safe to call from writer thread */
  bool isEmpty() const noexcept { return getAvailableWrite() >= getSize(); }

 private:
  size_type getAvailableWrite(size_type writeIndex,
                              size_type readIndex) const noexcept {
    auto result = readIndex - writeIndex - 1;
    if (writeIndex >= readIndex) result += mAllocatedSize;

    return result;
  }

  size_type getAvailableRead(size_type writeIndex,
                             size_type readIndex) const noexcept {
    if (writeIndex >= readIndex) return writeIndex - readIndex;

    return writeIndex + mAllocatedSize - readIndex;
  }

 private:
  std::vector<sample_t> mDataStore;
  sample_t* mData = nullptr;
  size_type mAllocatedSize = 0ULL;
  std::atomic<size_type> mWriteIndex = 0ULL, mReadIndex = 0ULL;
};
