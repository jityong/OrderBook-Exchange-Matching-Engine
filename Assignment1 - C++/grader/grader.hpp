#ifndef GRADER_HPP
#define GRADER_HPP

#include <cassert>
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>

#include <atomic>
#include <barrier>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <latch>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <semaphore>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

enum class EngineCommandType { Buy = 'B', Sell = 'S', Cancel = 'C' };

struct EngineCommand {
  EngineCommandType type;
  uint32_t order_id;
  uint32_t price;
  uint32_t count;
  char instrument[9];

  friend std::ostream &operator<<(std::ostream &os,
                                  const EngineCommand &i) {
    switch (i.type) {
      case EngineCommandType::Sell:
      case EngineCommandType::Buy:
        os << (i.type == EngineCommandType::Sell ? "Sell " : "Buy ")
           << i.instrument << " @ " << i.price << " x " << i.count
           << " ID " << i.order_id;
        break;
      case EngineCommandType::Cancel:
        os << "Cancel " << i.order_id;
        break;
      default:
        assert(false);
        break;
    }
    return os;
  }
};
static_assert(sizeof(EngineCommand) == 28, "Wrong size of EngineCommand");

enum class EngineOutputType : char {
  Buy = 'B',
  Sell = 'S',
  Exec = 'E',
  Cancel = 'X',
  Invalid = '!'
};
enum class CancelType : char { Accept = 'A', Reject = 'R' };

struct EngineOutputBuySell {
  uint32_t order_id;
  char instrument[9];
  uint32_t price;
  uint32_t count;
};

struct EngineOutputExecuted {
  uint32_t resting_order_id;
  uint32_t new_order_id;
  uint32_t execution_id;  // of resting order
  uint32_t price;         // of resting order
  uint32_t count;
};

struct EngineOutputCancel {
  uint32_t order_id;
  CancelType cancel_type;
};

struct ParsedEngineOutput {
  EngineOutputType type{EngineOutputType::Invalid};
  std::string line;
  union {
    EngineOutputBuySell buysell;
    EngineOutputExecuted exec;
    EngineOutputCancel cancel;
  };
  uintmax_t input_timestamp;
  uintmax_t output_timestamp;
};

ParsedEngineOutput parse_engine_output_line(std::string_view line);

enum class GraderInputType {
  Sync = '.',
  Connect = 'o',
  Disconnect = 'x',
  Sleep = 's',
  SendCommand = 'C',
  Wait = 'w',
  Invalid = '@'
};

struct GraderInput {
  GraderInputType type{GraderInputType::Invalid};
  union {
    uint32_t duration_ms;     // for Sleep
    EngineCommand command;    // for SendCommand
    uint32_t order_id;        // for Wait
    std::barrier<> *barrier;  // for Sync
  };
};

std::ostream &operator<<(std::ostream &, const GraderInput &);

typedef std::pair<std::optional<std::vector<size_t>>, GraderInput>
    ParsedGraderInput;

ParsedGraderInput parse_input_line(std::string &line, size_t num_threads);

enum class OrderState { Active, Booked, Filled };

struct OrderStatus {
  // Only buy or sell, not cancel
  EngineCommandType type;
  uint32_t order_id;
  uint32_t price;
  uint32_t count;
  uint32_t filled_count;
  uintmax_t input_timestamp;
  uintmax_t added_to_book_timestamp;
  std::string instrument;
  OrderState state;
  uint32_t execution_id;

  OrderStatus(EngineCommandType type, uint32_t order_id, uint32_t price,
              uint32_t count, std::string instrument)
      : type{type},
        order_id{order_id},
        price{price},
        count{count},
        filled_count{0},
        instrument{std::move(instrument)},
        state{OrderState::Active},
        execution_id{0} {
    assert(type != EngineCommandType::Cancel);
  }
};

class GradingSession {
  // We use semaphore instead of latch because semaphore has
  // sem.try_acquire_for(timeout)
  std::unordered_map<uint32_t, std::counting_semaphore<>>
      order_completion_latches{};
  std::unordered_map<uint32_t, OrderStatus> orders{};
  std::vector<std::vector<GraderInput>> thread_commands{};
  std::string socket_path;
  std::string tempdir_path;
  pid_t engine_pid{-1};
  FILE *stdout_file;
  FILE *stderr_file;
  [[maybe_unused]] size_t num_threads;

  // Identify when commands we sent were properly handled by output thread
  // This is harder for Cancel, but since Cancels can only be sent by
  // one thread, this is actually fine
  // This is how we know that all the output has arrived,
  // and to start checking for correctness.
  // We also use the buysell_count to know when to count_down the latches.
  std::mutex active_orders_mut;
  std::condition_variable active_orders_cond;
  std::unordered_map<uint32_t, uint32_t> active_buysell_count;
  std::unordered_set<uint32_t> active_cancel;

  // Exit conditions
  std::mutex exit_mut;
  std::condition_variable exit_cond;
  size_t num_threads_done{0};
  bool output_thread_done{false};
  bool failed{false};
  bool might_not_be_a_bug{false};

 private:
  explicit GradingSession(size_t num_threads)
      : num_threads{num_threads} {}
  void client_thread(size_t thread_id);
  void output_thread(size_t max_output_lines);

 public:
  GradingSession() = delete;
  ~GradingSession();
  static void run(size_t num_threads,
                  std::vector<ParsedGraderInput> commands,
                  std::string path);
};

struct SyncCerr {
  static std::mutex mut;
  std::scoped_lock<std::mutex> lock{SyncCerr::mut};

  template <typename T>
  friend const SyncCerr &operator<<(const SyncCerr &s, T &&v) {
    std::cerr << std::forward<T>(v);
    return s;
  }

  friend const SyncCerr &operator<<(const SyncCerr &s,
                                    std::ostream &(*f)(std::ostream &)) {
    std::cerr << f;
    return s;
  }
};

#endif
