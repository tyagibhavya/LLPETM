#pragma once

#include <iostream>
#include <vector>
// Each instantiation and full specialization of the std::atomic template defines an atomic type.
// If one thread writes to an atomic object while another thread reads from it, the behavior is well-defined.
#include <atomic>

#include "macros.h"

namespace Common {
  template<typename T>
  class LFQueue final {
  private:
    std::vector<T> store_;
    std::atomic<size_t> next_write_index_ = {0};
    std::atomic<size_t> next_read_index_ = {0};
    std::atomic<size_t> num_elements_ = {0};

  public:
    LFQueue(std::size_t num_elems) :
        store_(num_elems, T()) /* pre-allocation of vector storage. */ {
       /* Dynamically allocate the memory for the entire vector in the constructor.
        We can extend this design to allow the lock-free queue to be resized at runtime */
    }

    auto getNextToWriteTo() noexcept {
      /*  returns a pointer to the next element to write new data to. */
      return &store_[next_write_index_];
    }

    auto updateWriteIndex() noexcept {
      /* increments the write index (provide the user with a pointer to the element)
       if the objects are quite large then not all of it needs to be updated or overwritten*/
      next_write_index_ = (next_write_index_ + 1) % store_.size();
      num_elements_++;
    }

    auto getNextToRead() const noexcept -> const T * {
      /*  returns a pointer to the next element to be consumed but does not update the read index. */
      return (size() ? &store_[next_read_index_] : nullptr);
    }

    auto updateReadIndex() noexcept {
      /*  updates the read index after the element is consumed */
      next_read_index_ = (next_read_index_ + 1) % store_.size();
      ASSERT(num_elements_ != 0, "Read an invalid element in:" + std::to_string(pthread_self()));
      num_elements_--;
    }

    auto size() const noexcept {
      return num_elements_.load();
    }

    // Deleted default, copy & move constructors and assignment-operators.
    LFQueue() = delete;

    LFQueue(const LFQueue &) = delete;

    LFQueue(const LFQueue &&) = delete;

    LFQueue &operator=(const LFQueue &) = delete;

    LFQueue &operator=(const LFQueue &&) = delete;
  };
}
