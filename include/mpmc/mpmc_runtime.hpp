#pragma once

#include <mpmc/mpmc.hpp>
#include <proto/protocols.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include <core/utils.hpp>

namespace hft::mpmc {

inline proto::TaggedMessage make_test_message(std::uint64_t sequence) {
  proto::TaggedMessage msg{};
  msg.kind = proto::Kind::itch;
  proto::ItchAddOrder body{};
  body.stock_locate = static_cast<std::uint16_t>(sequence & 0xFFFFU);
  body.shares = static_cast<std::uint32_t>(sequence);
  std::memcpy(msg.bytes.data(), &body, sizeof(body));
  return msg;
}

inline std::uint64_t sequence_from(const proto::TaggedMessage &msg) {
  proto::ItchAddOrder body{};
  std::memcpy(&body, msg.bytes.data(), sizeof(body));
  return body.shares;
}

enum class Role : std::uint8_t { producer, consumer };

template <int Capacity>
class Runtime {
public:
  explicit Runtime(hft::MPMC<Capacity> &queue) : queue_(queue) {}

  Runtime(const Runtime &) = delete;
  Runtime &operator=(const Runtime &) = delete;

  void start(std::uint32_t seed = 42) {
    stop();
    rng_.seed(seed);
    running_.store(true, std::memory_order_release);
    supervisor_ = std::thread([this] { supervise(); });
  }

  void stop() {
    running_.store(false, std::memory_order_release);
    if (supervisor_.joinable()) {
      supervisor_.join();
    }
    join_all();
  }

  [[nodiscard]] std::uint64_t pushed() const noexcept {
    return pushed_.load(std::memory_order_acquire);
  }

  [[nodiscard]] std::uint64_t popped() const noexcept {
    return popped_.load(std::memory_order_acquire);
  }

  [[nodiscard]] std::size_t live_threads() const noexcept {
    return live_threads_.load(std::memory_order_acquire);
  }

private:
  struct Worker {
    std::thread thread;
    Role role{Role::producer};
    std::shared_ptr<std::atomic<bool>> done;
  };

  void supervise() {
    using clock = std::chrono::steady_clock;
    auto next_tick = clock::now();

    while (running_.load(std::memory_order_acquire)) {
      std::uniform_int_distribution<int> roll(0, 99);
      const int action = roll(rng_);

      if (action < 30) {
        try_join_random();
      }
      if (action < 60) {
        try_spawn(Role::producer);
      } else if (action < 90) {
        try_spawn(Role::consumer);
      }

      next_tick += std::chrono::milliseconds(5);
      std::this_thread::sleep_until(next_tick);
    }
  }

  void try_spawn(Role role) {
    if (workers_.size() >= max_live_threads_) {
      return;
    }

    Worker worker{};
    worker.role = role;
    worker.done = std::make_shared<std::atomic<bool>>(false);
    const std::uint32_t seed = static_cast<std::uint32_t>(
        next_sequence_.fetch_add(1, std::memory_order_relaxed));

    if (role == Role::producer) {
      worker.thread = std::thread([this, done = worker.done, seed] {
        producer_main(done, seed);
      });
    } else {
      worker.thread = std::thread([this, done = worker.done, seed] {
        consumer_main(done, seed);
      });
    }

    workers_.push_back(std::move(worker));
    live_threads_.fetch_add(1, std::memory_order_relaxed);
  }

  void try_join_random() {
    if (workers_.empty()) {
      return;
    }

    std::uniform_int_distribution<std::size_t> pick(0, workers_.size() - 1);
    const std::size_t index = pick(rng_);
    Worker &worker = workers_[index];
    if (!worker.done->load(std::memory_order_acquire)) {
      return;
    }

    if (worker.thread.joinable()) {
      worker.thread.join();
    }

    workers_[index] = std::move(workers_.back());
    workers_.pop_back();
    live_threads_.fetch_sub(1, std::memory_order_relaxed);
  }

  void join_all() {
    for (Worker &worker : workers_) {
      if (worker.thread.joinable()) {
        worker.thread.join();
      }
    }
    workers_.clear();
    live_threads_.store(0, std::memory_order_relaxed);
  }

  void producer_main(const std::shared_ptr<std::atomic<bool>> &done,
                     std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> ops_dist(1, 12);
    std::uniform_int_distribution<int> pause_dist(0, 200);

    const int ops = ops_dist(rng);
    for (int i = 0; i < ops; ++i) {
      const std::uint64_t sequence =
          next_sequence_.fetch_add(1, std::memory_order_relaxed);
      queue_.push(make_test_message(sequence));
      pushed_.fetch_add(1, std::memory_order_relaxed);
      if (pause_dist(rng) > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(pause_dist(rng)));
      }
    }

    done->store(true, std::memory_order_release);
  }

  void consumer_main(const std::shared_ptr<std::atomic<bool>> &done,
                     std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> ops_dist(1, 12);
    std::uniform_int_distribution<int> pause_dist(0, 200);
    std::uniform_int_distribution<int> idle_dist(0, 50);

    const int ops = ops_dist(rng);
    int consumed = 0;
    while (consumed < ops) {
      proto::TaggedMessage msg{};
      if (queue_.try_pop(msg)) {
        (void)sequence_from(msg);
        popped_.fetch_add(1, std::memory_order_relaxed);
        ++consumed;
        if (pause_dist(rng) > 0) {
          std::this_thread::sleep_for(std::chrono::microseconds(pause_dist(rng)));
        }
      } else {
        cpu_pause();
        if (idle_dist(rng) == 0) {
          std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
      }
    }

    done->store(true, std::memory_order_release);
  }

  hft::MPMC<Capacity> &queue_;
  std::atomic<bool> running_{false};
  std::atomic<std::uint64_t> pushed_{0};
  std::atomic<std::uint64_t> popped_{0};
  std::atomic<std::uint64_t> next_sequence_{1};
  std::atomic<std::size_t> live_threads_{0};
  std::mt19937 rng_{42};
  std::thread supervisor_;
  std::vector<Worker> workers_;
  std::size_t max_live_threads_{32};
};

} // namespace hft::mpmc
