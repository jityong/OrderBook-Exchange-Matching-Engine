#include "grader.hpp"

#include <fcntl.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <barrier>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_map>

std::mutex SyncCerr::mut;

static std::vector<size_t> range(size_t to) {
  std::vector<size_t> v;
  v.reserve(to);
  for (size_t i = 0; i < to; ++i) {
    v.push_back(i);
  }
  return v;
}

template <typename F>
static void go(F &&f) {
  std::thread{std::forward<F>(f)}.detach();
};

static void check(bool condition, const char *fail_reason) {
  if (!condition) {
    throw std::runtime_error(fail_reason);
  }
}

static auto &at(auto &orders, auto order_id, const char *fail_reason) {
  try {
    return orders.at(order_id);
  } catch (...) {
    throw std::runtime_error(fail_reason);
  }
}

template <typename Start, typename End>
struct iter_pair_range {
  Start start;
  End last;
  iter_pair_range(std::pair<Start, End> p)
      : start(p.first), last(p.second) {}
  Start begin() const { return start; }
  End end() const { return last; }
};

template <typename Start, typename End>
iter_pair_range(std::pair<Start, End>) -> iter_pair_range<Start, End>;

static void validate_commands(size_t num_threads,
                              std::vector<ParsedGraderInput> &commands) {
  struct ThreadConnection {
    size_t thread;
    size_t connection;
  };

  struct ThreadState {
    bool connected{false};
    size_t connection_id{0};
  };

  bool failed = false;

  std::unordered_map<uint32_t, ThreadConnection>
      seen_ids;  // order id -> thread/connection
  std::unordered_map<uint32_t, ThreadState>
      threads;  // thread id -> thread state
  std::vector<size_t> all_threads_vector = range(num_threads);

  for (size_t i = 0; i < num_threads; ++i) {
    threads.emplace(i, ThreadState{});
  }

  for (const auto &command_pair : commands) {
    auto &command = command_pair.second;
    auto &command_threads = command_pair.first.has_value()
                                ? command_pair.first.value()
                                : all_threads_vector;
    switch (command.type) {
      case GraderInputType::SendCommand: {
        if (command_threads.size() != 1) {
          SyncCerr{} << "Invalid input: '" << command << "' from "
                     << command_threads.size() << " threads."
                     << std::endl;
          failed = true;
        } else {
          size_t thread_id = command_threads[0];
          if (!threads[thread_id].connected) {
            SyncCerr{} << "Invalid input: thread " << thread_id << " '"
                       << command << "' while not connected" << std::endl;
            failed = true;
          }
          switch (command.command.type) {
            case EngineCommandType::Buy:
            case EngineCommandType::Sell:
              if (seen_ids.contains(command.command.order_id)) {
                ThreadConnection &first_seen =
                    seen_ids[command.command.order_id];
                SyncCerr{} << "Invalid input: repeated order ID "
                           << command.command.order_id << " on thread "
                           << thread_id << ", first seen on thread "
                           << first_seen.thread << " connection "
                           << first_seen.connection << std::endl;
                failed = true;
              } else {
                seen_ids.emplace(
                    command.command.order_id,
                    ThreadConnection{
                        .thread = thread_id,
                        .connection = threads[thread_id].connection_id});
              }
              break;
            case EngineCommandType::Cancel:
              if (seen_ids.contains(command.command.order_id)) {
                ThreadConnection &first_seen =
                    seen_ids[command.command.order_id];
                if (first_seen.thread != thread_id ||
                    first_seen.connection !=
                        threads[thread_id].connection_id) {
                  SyncCerr{} << "Invalid input: cancel for order ID "
                             << command.command.order_id << " on thread "
                             << thread_id << " connection "
                             << threads[thread_id].connection_id
                             << ", but order was created on thread "
                             << first_seen.thread << " connection "
                             << first_seen.connection << std::endl;
                  failed = true;
                }
              } else {
                SyncCerr{}
                    << "Invalid input: cancel for yet-unknown order ID "
                    << command.command.order_id << " on thread "
                    << thread_id << std::endl;
                failed = true;
              }
              break;
          }
        }
        break;
      }
      case GraderInputType::Connect:
        for (size_t thread : command_threads) {
          auto &state = threads[thread];
          if (state.connected) {
            SyncCerr{} << "Invalid input: thread " << thread
                       << " connecting while connected" << std::endl;
            failed = true;
          }
          state.connected = true;
        }
        break;
      case GraderInputType::Disconnect:
        for (size_t thread : command_threads) {
          auto &state = threads[thread];
          if (!state.connected) {
            SyncCerr{} << "Invalid input: thread " << thread
                       << " disconnecting while not connected"
                       << std::endl;
            failed = true;
          }
          state.connected = false;
          state.connection_id++;
        }
        break;
      case GraderInputType::Invalid:
        SyncCerr{} << "Invalid input: invalid command seen" << std::endl;
        failed = true;
        break;
      case GraderInputType::Sync:
      case GraderInputType::Wait:
      case GraderInputType::Sleep:
        // no-op
        break;
    }
  }

  assert(!failed);
}

void GradingSession::run(size_t num_threads,
                         std::vector<ParsedGraderInput> commands,
                         std::string path) {
  signal(SIGPIPE, SIG_IGN);
  validate_commands(num_threads, commands);

  GradingSession g{num_threads};
  for (size_t i = 0; i < num_threads; ++i) {
    g.thread_commands.emplace_back();
  }

  // initialise state
  {
    std::vector<size_t> all_threads_vector = range(num_threads);
    for (auto &command_pair : commands) {
      auto &command = command_pair.second;
      auto &command_threads = command_pair.first.has_value()
                                  ? command_pair.first.value()
                                  : all_threads_vector;
      switch (command.type) {
        case GraderInputType::SendCommand:
          switch (command.command.type) {
            case EngineCommandType::Buy:
            case EngineCommandType::Sell: {
              assert(g.order_completion_latches
                         .try_emplace(command.command.order_id, 0)
                         .second);
              assert(g.orders
                         .try_emplace(command.command.order_id,
                                      command.command.type,
                                      command.command.order_id,
                                      command.command.price,
                                      command.command.count,
                                      command.command.instrument)
                         .second);
              break;
            }
            case EngineCommandType::Cancel:
              break;
          }
          break;

        case GraderInputType::Sync:
          command.barrier = new std::barrier{
              static_cast<ptrdiff_t>(command_threads.size())};
          break;
        case GraderInputType::Wait:
        case GraderInputType::Connect:
        case GraderInputType::Disconnect:
        case GraderInputType::Sleep:
        case GraderInputType::Invalid:
          break;
      }
      for (auto thread : command_threads) {
        g.thread_commands[thread].push_back(command);
      }
    }
  }

  // launch engine
  {
    g.tempdir_path =
        "/tmp/cs3211a1-" + std::to_string(getpid()) + "-XXXXXX";
    assert(mkdtemp(g.tempdir_path.data()));
    // from this point forward we cannot assert, as we want our dtor to
    // run
    g.socket_path =
        g.tempdir_path + "/socket-" + std::to_string(getpid());

    int stdout_pipefd[2];
    if (pipe(stdout_pipefd)) {
      throw std::runtime_error("pipe() failed");
    }
    int stderr_pipefd[2];
    if (pipe(stderr_pipefd)) {
      throw std::runtime_error("pipe() failed");
    }
    pid_t engine_pid = fork();
    if (!engine_pid) {
      if (prctl(PR_SET_PDEATHSIG, SIGKILL)) {
        perror("prctl");
        _Exit(1);
      }
      dup2(stdout_pipefd[1], 1);
      close(stdout_pipefd[0]);
      close(stdout_pipefd[1]);
      dup2(stderr_pipefd[1], 2);
      close(stderr_pipefd[0]);
      close(stderr_pipefd[1]);
      close(0);
      open("/dev/null", O_WRONLY);
      dup2(0, 2);
      execl(path.c_str(), path.c_str(), g.socket_path.c_str(), nullptr);
      perror("execl");
      _Exit(1);
    }
    if (engine_pid == -1) {
      throw std::runtime_error("fork() failed");
    }
    close(stdout_pipefd[1]);
    close(stderr_pipefd[1]);
    g.stdout_file = fdopen(stdout_pipefd[0], "rb");
    g.stderr_file = fdopen(stderr_pipefd[0], "rb");
    g.engine_pid = engine_pid;
  }

  // stderr echoer thread
  go([&g]() {
    char buf[4096];
    do {
      if (fgets(buf, 4096, g.stderr_file) != nullptr) {
        SyncCerr{} << "Engine stderr: " << buf << std::flush;
      }
    } while (ferror(g.stderr_file) == 0 && feof(g.stderr_file) == 0);
      SyncCerr{} << "Engine stderr closed\n" << std::flush;
  });

  // stdout checker thread
  go([&g, commands_size = commands.size()]() {
    try {
      // every output line corresponds to at most 2 lines
      // cancels: exactly 1
      // buysell: at most 2, 1 when it enters book, 1 more when it gets
      //          fully filled
      // execute:
      //  - partially fulfils resting: at most 1 line,
      //                               since new order is fully matched
      //  - fully fulfils resting: 0 lines + remaining lines,
      //                           since we consider this line
      //                           as belonging to the resting order
      // total count of all execute lines is at most 0+0+0+...+1 = 1
      g.output_thread(commands_size * 2);
      {
        std::scoped_lock lock{g.exit_mut};
        g.output_thread_done = true;
      }
      g.exit_cond.notify_one();
    } catch (const std::exception &error) {
      std::string err =
          std::string("Output thread exception: ") + error.what() + "\n";
      SyncCerr{} << err;
      {
        std::scoped_lock lock{g.exit_mut};
        g.failed = true;
        if (std::string{error.what()}.find("might not be a bug") !=
            std::string::npos) {
          g.might_not_be_a_bug = true;
        }
      }
      g.exit_cond.notify_one();
    }
  });

  // Wait a while before starting clients
  std::this_thread::sleep_for(std::chrono::milliseconds{50});

  // client threads
  for (size_t i = 0; i < num_threads; ++i) {
    go([i, &g]() {
      try {
        g.client_thread(i);
        {
          std::scoped_lock lock{g.exit_mut};
          g.num_threads_done++;
        }
        g.active_orders_cond.notify_one();
        g.exit_cond.notify_one();
      } catch (const std::exception &error) {
        std::string err = "Client thread " + std::to_string(i) +
                          " exception: " + error.what() + "\n";
        SyncCerr{} << err;
        {
          std::scoped_lock lock{g.exit_mut};
          g.failed = true;
        }
        g.exit_cond.notify_one();
      }
    });
  }

  {
    std::unique_lock lock{g.exit_mut};
    g.exit_cond.wait(lock, [&g]() {
      return g.failed || (g.num_threads_done == g.num_threads &&
                          g.output_thread_done);
    });
  }

  if (g.failed) {
    SyncCerr{} << "One or more threads failed" << std::endl;
    // We exit without calling destructors as threads may still have
    // dangling references to `g`.
    if (g.might_not_be_a_bug) {
      exit(2);
    } else {
      exit(1);
    }
  }
}

void GradingSession::client_thread(size_t thread_id) {
  int fd = -1;
  for (auto &command : thread_commands[thread_id]) {
    switch (command.type) {
      case GraderInputType::Wait: {
        if (order_completion_latches.at(command.order_id)
                .try_acquire_for(std::chrono::milliseconds{100})) {
          break;
        }
        SyncCerr{} << "Client " << thread_id << ": Waiting for order "
                   << command.order_id << " took more than 100ms"
                   << std::endl;
        if (order_completion_latches.at(command.order_id)
                .try_acquire_for(std::chrono::milliseconds{900})) {
          break;
        }
        SyncCerr{} << "Client " << thread_id << ": Waiting for order "
                   << command.order_id
                   << " took more than 1000ms. Possible deadlock, or "
                      "server does not flush stdout"
                   << std::endl;
        order_completion_latches.at(command.order_id).acquire();
        break;
      }
      case GraderInputType::Sync:
        command.barrier->arrive_and_wait();
        break;
      case GraderInputType::Sleep:
        std::this_thread::sleep_for(
            std::chrono::milliseconds(command.duration_ms));
        break;
      case GraderInputType::Connect: {
        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd == -1) {
          perror("socket");
          throw std::runtime_error("socket() failed");
        }

        {
          struct sockaddr_un sockaddr = {.sun_family = AF_UNIX};
          strncpy(sockaddr.sun_path, socket_path.c_str(),
                  sizeof(sockaddr.sun_path) - 1);
          while (connect(fd,
                         reinterpret_cast<struct sockaddr *>(&sockaddr),
                         sizeof(sockaddr)) != 0) {
            std::string error = "Client thread " +
                                std::to_string(thread_id) +
                                " connect() failed: " + strerror(errno) +
                                "; retrying in 100ms...\n";
            SyncCerr{} << error;
            SyncCerr{} << socket_path << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
          }
        }

        break;
      }

      case GraderInputType::Disconnect: {
        close(fd);
        fd = -1;
        break;
      }

      case GraderInputType::SendCommand: {
        /* SyncCerr{} << "Sending on thread " << thread_id << ": " */
        /*            << command.command << std::endl; */
        switch (command.command.type) {
          case EngineCommandType::Buy:
          case EngineCommandType::Sell: {
            std::scoped_lock lock{active_orders_mut};
            active_buysell_count[command.command.order_id] =
                command.command.count;
            break;
          }
          case EngineCommandType::Cancel: {
            std::scoped_lock lock{active_orders_mut};
            active_cancel.insert(command.command.order_id);
            break;
          }
        }
        active_orders_cond.notify_one();
        write(fd, &command.command, sizeof(command.command));
        break;
      }

      case GraderInputType::Invalid:
        break;
    }
  }
  if (fd >= 0) {
    close(fd);
  }
}

enum class PriorityType {
  PriceTime,  // After clarification, this was allowed
  Time,       // Original priority type in assignment writeup
};

enum class SortByTimestampType {
  Input,        // Interpreted by many students
  AddedToBook,  // Intended
};

template <PriorityType PType, SortByTimestampType SType>
struct InstBook {
  struct BuyOrdering {
    bool operator()(OrderStatus *buy1, OrderStatus *buy2) const {
      if constexpr (PType == PriorityType::PriceTime) {
        // return true if buy1 is better, i.e. should be sorted earlier.
        // So for us, this means that we return true if buy1 has a higher
        // price, or if they're equal, if buy1 came earlier.
        if constexpr (SType == SortByTimestampType::AddedToBook) {
          return buy1->price > buy2->price ||
                 (buy1->price == buy2->price &&
                  buy1->added_to_book_timestamp <
                      buy2->added_to_book_timestamp);
        } else {
          return buy1->price > buy2->price ||
                 (buy1->price == buy2->price &&
                  buy1->input_timestamp < buy2->input_timestamp);
        }
      } else {
        return buy1->price > buy2->price;
      }
    }
  };

  struct SellOrdering {
    bool operator()(OrderStatus *sell1, OrderStatus *sell2) const {
      if constexpr (PType == PriorityType::PriceTime) {
        // return true if sell1 is better, i.e. should be sorted earlier.
        // So for us, this means that we return true if sell1 has a higher
        // price, or if they're equal, if sell1 came earlier.
        if constexpr (SType == SortByTimestampType::AddedToBook) {
          return sell1->price < sell2->price ||
                 (sell1->price == sell2->price &&
                  sell1->added_to_book_timestamp <
                      sell2->added_to_book_timestamp);
        } else {
          return sell1->price < sell2->price ||
                 (sell1->price == sell2->price &&
                  sell1->input_timestamp < sell2->input_timestamp);
        }
      } else {
        return sell1->price < sell2->price;
      }
    }
  };

  std::multiset<OrderStatus *, BuyOrdering> buys;
  std::multiset<OrderStatus *, SellOrdering> sells;
  std::unordered_map<uint32_t, OrderStatus *> all_orders;

  void execute_buy(const EngineOutputExecuted &exec_buy) {
    check(!sells.empty(),
          "executed buy order when order book has no sells");
    check((*sells.begin())->price <= exec_buy.price,
          "executed buy order with lower price than sellers");
    // If there are multiple matchable orders (same timestamp!) we will be
    // lenient and allow either to be matched. whichever is chosen, we
    // will obey.

    auto resting_order = at(all_orders, exec_buy.resting_order_id,
                            "matched with resting order not in book");

    resting_order->filled_count += exec_buy.count;

    if constexpr (PType == PriorityType::PriceTime) {
      // For price-time priority, simply match against
      // (one of) the best orders in terms of price+time sorting
      bool found = false;

      for (auto it = sells.begin();
           it != sells.upper_bound(*sells.begin()); ++it) {
        if (resting_order->order_id == (*it)->order_id) {
          if (resting_order->filled_count == resting_order->count) {
            sells.erase(it);
          }
          found = true;
          break;
        }
      }

      if (!found) {
        std::stringstream err_msg;
        err_msg << "executed buy order with incorrect resting order, "
                   "expected one of";
        auto its = sells.equal_range(*sells.begin());
        for (const auto &b : iter_pair_range{its}) {
          err_msg << " " << b->order_id;
        }
        /* err_msg << " and not"; */
        /* for (const auto &b : */
        /*      iter_pair_range{sells.equal_range(*its.second)}) { */
        /*   err_msg << " " << b->order_id; */
        /* } */
        err_msg << " but got " << exec_buy.resting_order_id;
        check(false, err_msg.str().c_str());
      }
    } else {
      // For time priority, loop over all matchable orders, and ensure
      // that we matched against the earliest one.
      OrderStatus fake_sell_order{EngineCommandType::Sell, 0,
                                  exec_buy.price, 0, ""};
      for (auto it = sells.begin();
           it != sells.upper_bound(&fake_sell_order);) {
        if constexpr (SType == SortByTimestampType::AddedToBook) {
          check((*it)->added_to_book_timestamp >=
                    resting_order->added_to_book_timestamp,
                "matched with resting order that was not the earliest "
                "matchable order");
        } else {
          check((*it)->input_timestamp >= resting_order->input_timestamp,
                "matched with resting order that was not the earliest "
                "matchable order");
        }

        if (resting_order->filled_count == resting_order->count &&
            resting_order->order_id == (*it)->order_id) {
          it = sells.erase(it);
        } else {
          ++it;
        }
      }
    }

    // Checks passed, apply changes to resting order

    if (resting_order->filled_count == resting_order->count) {
      all_orders.erase(exec_buy.resting_order_id);
    }

    resting_order->execution_id++;

    check(resting_order->execution_id == exec_buy.execution_id,
          ("incorrect execution id, expected " +
           std::to_string(resting_order->execution_id) + " but got " +
           std::to_string(exec_buy.execution_id))
              .c_str());
  }

  void execute_sell(const EngineOutputExecuted &exec_sell) {
    check(!buys.empty(),
          "executed sell order when order book has no buys");
    check((*buys.begin())->price >= exec_sell.price,
          "executed sell order with higher price than buyers");
    // If there are multiple matchable orders (same timestamp!) we will be
    // lenient and allow either to be matched. whichever is chosen, we
    // will obey.

    auto resting_order = at(all_orders, exec_sell.resting_order_id,
                            "matched with resting order not in book");

    resting_order->filled_count += exec_sell.count;

    if constexpr (PType == PriorityType::PriceTime) {
      // For price-time priority, simply match against
      // (one of) the best orders in terms of price+time sorting
      bool found = false;

      for (auto it = buys.begin(); it != buys.upper_bound(*buys.begin());
           ++it) {
        if (resting_order->order_id == (*it)->order_id) {
          if (resting_order->filled_count == resting_order->count) {
            buys.erase(it);
          }
          found = true;
          break;
        }
      }

      if (!found) {
        std::stringstream err_msg;
        err_msg << "executed sell order with incorrect resting order, "
                   "expected one of";
        auto its = buys.equal_range(*buys.begin());
        for (const auto &b : iter_pair_range{its}) {
          err_msg << " " << b->order_id;
        }
        /* err_msg << " and not"; */
        /* for (const auto &b : */
        /*      iter_pair_range{buys.equal_range(*its.second)}) { */
        /*   err_msg << " " << b->order_id; */
        /* } */
        err_msg << " but got " << exec_sell.resting_order_id;
        check(false, err_msg.str().c_str());
      }
    } else {
      // For time priority, loop over all matchable orders, and ensure
      // that we matched against the earliest one.
      OrderStatus fake_buy_order{EngineCommandType::Buy, 0,
                                 exec_sell.price, 0, ""};
      for (auto it = buys.begin();
           it != buys.upper_bound(&fake_buy_order);) {
        if constexpr (SType == SortByTimestampType::AddedToBook) {
          check((*it)->added_to_book_timestamp >=
                    resting_order->added_to_book_timestamp,
                "matched with resting order that was not the earliest "
                "matchable order");
        } else {
          check((*it)->input_timestamp >= resting_order->input_timestamp,
                "matched with resting order that was not the earliest "
                "matchable order");
        }

        if (resting_order->filled_count == resting_order->count &&
            resting_order->order_id == (*it)->order_id) {
          it = buys.erase(it);
        } else {
          ++it;
        }
      }
    }

    // Checks passed, apply changes to resting order

    if (resting_order->filled_count == resting_order->count) {
      all_orders.erase(exec_sell.resting_order_id);
    }

    resting_order->execution_id++;

    check(resting_order->execution_id == exec_sell.execution_id,
          ("incorrect execution id, expected " +
           std::to_string(resting_order->execution_id) + " but got " +
           std::to_string(exec_sell.execution_id))
              .c_str());
  }

  void add_buy(const EngineOutputBuySell &buy, OrderStatus *order) {
    if (!sells.empty()) {
      // Check that we can't match
      check((*sells.begin())->price > buy.price,
            "added buy to book when a matchable sell exists");
    }
    buys.insert(order);
    all_orders.try_emplace(buy.order_id, order);
  }

  void add_sell(const EngineOutputBuySell &sell, OrderStatus *order) {
    if (!buys.empty()) {
      // Check that we can't match
      check((*buys.begin())->price < sell.price,
            "added sell to book when a matchable buy exists");
    }
    sells.insert(order);
    all_orders.try_emplace(sell.order_id, order);
  }

  void cancel(const EngineOutputCancel &cancel) {
    switch (cancel.cancel_type) {
      case CancelType::Accept: {
        OrderStatus *order =
            at(all_orders, cancel.order_id,
               "accepted cancel of order that's not in book");
        order->filled_count = order->count;
        order->state = OrderState::Filled;
        all_orders.erase(cancel.order_id);
        if (order->type == EngineCommandType::Buy) {
          for (auto it = buys.lower_bound(order); it != buys.upper_bound(order); ++it) {
            if (order->order_id == (*it)->order_id) {
              buys.erase(it);
              break;
            }
          }
        } else {
          for (auto it = sells.lower_bound(order); it != sells.upper_bound(order); ++it) {
            if (order->order_id == (*it)->order_id) {
              sells.erase(it);
              break;
            }
          }
        }
        break;
      }
      case CancelType::Reject: {
        check(!all_orders.contains(cancel.order_id),
              "rejected cancel of order that's in book");
        break;
      }
    }
  }
};

static auto groupbytimestamp(
    const std::vector<ParsedEngineOutput> &outputs) {
  std::vector<
      std::vector<std::reference_wrapper<const ParsedEngineOutput>>>
      groups;
  if (outputs.empty()) {
    return groups;
  }
  groups.emplace_back();
  auto prev_timestamp = outputs.front().output_timestamp;
  for (const auto &output : outputs) {
    if (output.output_timestamp != prev_timestamp) {
      groups.emplace_back();
    }
    prev_timestamp = output.output_timestamp;
    groups.back().push_back(std::cref(output));
  }
  return groups;
}

/* static auto permutations( */
/*     std::vector<std::reference_wrapper<const ParsedEngineOutput>> */
/*         outputs) { */
/*   std::vector< */
/*       std::pair<int, std::reference_wrapper<const ParsedEngineOutput>>>
 */
/*       indexed_outputs; */
/*   int i = 0; */
/*   for (const auto &output : outputs) { */
/*     indexed_outputs.push_back(std::make_pair(i++, output)); */
/*   } */

/*   std::vector< */
/*       std::vector<std::reference_wrapper<const ParsedEngineOutput>>> */
/*       permutations; */

/*   while (true) { */
/*     permutations.emplace_back(); */
/*     for (const auto &[_, output] : indexed_outputs) { */
/*       permutations.back().push_back(output); */
/*     } */
/*     if (std::next_permutation(indexed_outputs.begin(), */
/*                               indexed_outputs.end(), */
/*                               [](const auto &a1, const auto &a2) { */
/*                                 return a1.first < a2.first; */
/*                               })) { */
/*       continue; */
/*     } else { */
/*       break; */
/*     } */
/*   } */

/*   return permutations; */
/* } */

template <PriorityType PType, SortByTimestampType SType>
static void check_correctness_outputset(
    std::unordered_map<uint32_t, OrderStatus> &orders,
    std::unordered_map<std::string, InstBook<PType, SType>> &book,
    const std::vector<std::reference_wrapper<const ParsedEngineOutput>>
        &outputset) {
  for (const auto &output_ref : outputset) {
    const auto &output = output_ref.get();
    std::cout << "Checking: " << output.line << std::flush;
    switch (output.type) {
      case EngineOutputType::Buy:
      case EngineOutputType::Sell: {
        auto &status =
            at(orders, output.buysell.order_id, "order does not exist");
        auto correct_side = output.type == EngineOutputType::Buy
                                ? EngineCommandType::Buy
                                : EngineCommandType::Sell;

        check(status.state == OrderState::Active,
              "booking inactive order");
        check(status.type == correct_side, "incorrect side");
        check(status.filled_count < status.count,
              "booking fully filled order");
        check(std::strcmp(status.instrument.c_str(),
                          output.buysell.instrument) == 0,
              "incorrect instrument");
        check(status.price == output.buysell.price, "incorrect price");

        status.state = OrderState::Booked;
        status.input_timestamp = output.input_timestamp;
        status.added_to_book_timestamp = output.output_timestamp;

        if (output.type == EngineOutputType::Buy) {
          book[status.instrument].add_buy(output.buysell, &status);
        } else {
          book[status.instrument].add_sell(output.buysell, &status);
        }

        break;
      }

      case EngineOutputType::Exec: {
        auto &resting_status = at(orders, output.exec.resting_order_id,
                                  "resting order does not exist");
        auto &new_status = at(orders, output.exec.new_order_id,
                              "new order does not exist");

        check(resting_status.state == OrderState::Booked,
              "resting order not in book");
        check(new_status.state == OrderState::Active,
              "new order not active");
        check(resting_status.price == output.exec.price,
              "incorrect price");
        check(resting_status.instrument == new_status.instrument,
              "matched orders for different instruments");

        if (new_status.type == EngineCommandType::Buy) {
          book[resting_status.instrument].execute_buy(output.exec);
        } else {
          book[resting_status.instrument].execute_sell(output.exec);
        }

        new_status.filled_count += output.exec.count;

        check(resting_status.filled_count <= resting_status.count,
              "resting order filled more than initial quantity");
        check(new_status.filled_count <= new_status.count,
              "new order filled more than initial quantity");

        if (new_status.filled_count == new_status.count) {
          new_status.state = OrderState::Filled;
        }
        if (resting_status.filled_count == resting_status.count) {
          resting_status.state = OrderState::Filled;
        }

        break;
      }

      case EngineOutputType::Cancel: {
        auto &status = at(orders, output.cancel.order_id,
                          "cancel order does not exist");

        if (output.cancel.cancel_type == CancelType::Accept) {
          check(status.state == OrderState::Booked,
                "accepted cancel for order not in book");
        } else {
          check(status.state != OrderState::Booked,
                "rejected cancel for order in book");
        }

        book[status.instrument].cancel(output.cancel);

        break;
      }

      case EngineOutputType::Invalid: {
        assert(false);  // Should be filtered out already
        break;
      }
    }
  }
}

template <PriorityType PType, SortByTimestampType SType>
static void check_correctness(
    std::unordered_map<uint32_t, OrderStatus> orders,
    const std::vector<ParsedEngineOutput> &outputs) {
  std::unordered_map<std::string, InstBook<PType, SType>> book;

  for (const auto &outputset : groupbytimestamp(outputs)) {
    /* auto perms = permutations(outputset); */
    if (outputset.size() == 1) {
      check_correctness_outputset(orders, book, outputset);
    } else {
      try {
        check_correctness_outputset(orders, book, outputset);
      } catch (const std::runtime_error &err) {
        throw std::runtime_error(
            err.what() + std::string{", but there were "} +
            std::to_string(outputset.size()) +
            " outputs with the same "
            "timestamp, so this might not be a bug.");
      }
    }
  }
}

void GradingSession::output_thread(size_t max_output_lines) {
  std::vector<ParsedEngineOutput> outputs;
  outputs.reserve(max_output_lines);
  char buf[4096];
  while (ferror(stdout_file) == 0 && feof(stdout_file) == 0) {
    if (fgets(buf, 4096, stdout_file) == nullptr) {
      break;
    }
    std::string_view output_line{buf};
    if (output_line.starts_with("#") || output_line.empty() ||
        output_line.starts_with("Got ")) {
      // Be a little tolerant of comments and empty output lines because
      // our examples have them.
      // But don't bother being more tolerant for now unless we see
      // someone fail this.
      continue;
    }

    std::cout << "Engine stdout: " << output_line << std::flush;
    try {
      outputs.emplace_back(parse_engine_output_line(output_line));
    } catch (const std::exception& exc) {
      throw std::runtime_error(std::string{"Could not parse engine output: "} + exc.what());
    }
    const auto &output = outputs.back();

    switch (output.type) {
      case EngineOutputType::Buy:
      case EngineOutputType::Sell: {
        {
          std::scoped_lock lock{active_orders_mut};
          if (active_buysell_count.erase(output.buysell.order_id) == 0) {
            throw std::runtime_error(
                "got output for an order that should not exist (it has fully executed, or it was not sent yet)");
          }
        }
        at(order_completion_latches, output.buysell.order_id,
           "order does not exist")
            .release(num_threads);
        break;
      }

      case EngineOutputType::Exec: {
        bool do_count_down = false;
        {
          std::scoped_lock lock{active_orders_mut};
          auto &active_count =
              at(active_buysell_count, output.exec.new_order_id,
                 "new order does not exist");
          active_count -= output.exec.count;
          if (active_count < 0) {
            throw std::runtime_error("filled more than order count");
          } else if (active_count == 0) {
            active_buysell_count.erase(output.exec.new_order_id);
            do_count_down = true;
          }
        }
        if (do_count_down) {
          at(order_completion_latches, output.exec.new_order_id,
             "new order does not exist")
              .release(num_threads);
        }
        break;
      }

      case EngineOutputType::Cancel: {
        std::scoped_lock lock{active_orders_mut};
        if (active_cancel.erase(output.cancel.order_id) == 0) {
          throw std::runtime_error(
              "cancel output without corresponding cancel command");
        }
        break;
      }

      case EngineOutputType::Invalid: {
        throw std::runtime_error("invalid command");
        break;
      }
    }

    {
      std::unique_lock lock{active_orders_mut};
      active_orders_cond.wait(lock, [this]() {
        if (active_buysell_count.size() != 0 ||
            active_cancel.size() != 0) {
          return true;
        }

        std::scoped_lock lock{exit_mut};
        if (num_threads_done == num_threads) {
          return true;
        }

        return false;
      });

      if (active_buysell_count.size() == 0 && active_cancel.size() == 0 &&
          num_threads_done == num_threads) {
        break;
      }
    }
  }

  if (ferror(stdout_file) != 0) {
    throw std::runtime_error("Error reading output");
  }

  {
    std::scoped_lock lock{active_orders_mut};
    if (active_buysell_count.size() > 0 || active_cancel.size() > 0 ||
        num_threads_done != num_threads) {
      throw std::runtime_error(
          "engine closed despite orders still being active");
    }
  }

  std::vector<std::string> errors;
  // check for correctness
  // once a correctness test passes, we immediately return early

  std::vector<ParsedEngineOutput> outputs_sorted = outputs;
  std::stable_sort(
      outputs_sorted.begin(), outputs_sorted.end(),
      [](const ParsedEngineOutput &o1, const ParsedEngineOutput &o2) {
        return o1.output_timestamp < o2.output_timestamp;
      });

  try {
    std::cout << "Checking timestamp order correctness, price-time "
                 "priority, orders sorted by added-to-book timestamp"
              << std::endl;
    check_correctness<PriorityType::PriceTime,
                      SortByTimestampType::AddedToBook>(orders,
                                                        outputs_sorted);
    std::cout << "Correct using timestamp order correctness, price-time "
                 "priority, orders sorted by added-to-book timestamp"
              << std::endl;
    return;
  } catch (const std::exception &exc) {
    errors.push_back(
        std::string{"timestamp order, price-time priority, orders sorted "
                    "by added-to-book timestamp error: "} +
        exc.what());
  }

  try {
    std::cout << "Checking timestamp order correctness, price-time "
                 "priority, orders sorted by input timestamp"
              << std::endl;
    check_correctness<PriorityType::PriceTime,
                      SortByTimestampType::Input>(orders, outputs_sorted);
    std::cout << "Correct using timestamp order correctness, price-time "
                 "priority, orders sorted by input timestamp"
              << std::endl;
    return;
  } catch (const std::exception &exc) {
    errors.push_back(
        std::string{"timestamp order, price-time priority, orders sorted "
                    "by input timestamp error: "} +
        exc.what());
  }

  try {
    std::cout << "Checking output order correctness, price-time "
                 "priority, orders sorted by added-to-book timestamp"
              << std::endl;
    check_correctness<PriorityType::PriceTime,
                      SortByTimestampType::AddedToBook>(orders, outputs);
    std::cout << "Correct using output order correctness, price-time "
                 "priority, orders sorted by added-to-book timestamp"
              << std::endl;
    return;
  } catch (const std::exception &exc) {
    errors.push_back(
        std::string{"output order, price-time priority, orders sorted by "
                    "added-to-book timestamp error: "} +
        exc.what());
  }

  try {
    std::cout << "Checking output order correctness, price-time "
                 "priority, orders sorted by input timestamp"
              << std::endl;
    check_correctness<PriorityType::PriceTime,
                      SortByTimestampType::Input>(orders, outputs);
    std::cout << "Correct using output order correctness, price-time "
                 "priority, orders sorted by input timestamp"
              << std::endl;
    return;
  } catch (const std::exception &exc) {
    errors.push_back(
        std::string{"output order, price-time priority, orders sorted by "
                    "input timestamp error: "} +
        exc.what());
  }

  try {
    std::cout << "Checking timestamp order correctness, price-time "
                 "priority, orders sorted by added-to-book timestamp"
              << std::endl;
    check_correctness<PriorityType::Time,
                      SortByTimestampType::AddedToBook>(orders,
                                                        outputs_sorted);
    std::cout << "Correct using timestamp order correctness, price-time "
                 "priority, orders sorted by added-to-book timestamp"
              << std::endl;
    return;
  } catch (const std::exception &exc) {
    errors.push_back(
        std::string{"timestamp order, time priority, orders sorted by "
                    "added-to-book timestamp error: "} +
        exc.what());
  }

  try {
    std::cout << "Checking timestamp order correctness, price-time "
                 "priority, orders sorted by input timestamp"
              << std::endl;
    check_correctness<PriorityType::Time, SortByTimestampType::Input>(
        orders, outputs_sorted);
    std::cout << "Correct using timestamp order correctness, price-time "
                 "priority, orders sorted by input timestamp"
              << std::endl;
    return;
  } catch (const std::exception &exc) {
    errors.push_back(std::string{"timestamp order, time priority, orders "
                                 "sorted by input timestamp error: "} +
                     exc.what());
  }

  try {
    std::cout << "Checking output order correctness, time-priority, "
                 "orders sorted by added-to-book timestamp"
              << std::endl;
    check_correctness<PriorityType::Time,
                      SortByTimestampType::AddedToBook>(orders, outputs);
    std::cout << "Correct using output order correctness, time-priority, "
                 "orders sorted by added-to-book timestamp"
              << std::endl;
    return;
  } catch (const std::exception &exc) {
    errors.push_back(
        std::string{"output order, time priority, orders sorted by "
                    "added-to-book timestamp error: "} +
        exc.what());
  }

  try {
    std::cout << "Checking output order correctness, time-priority, "
                 "orders sorted by input timestamp"
              << std::endl;
    check_correctness<PriorityType::Time, SortByTimestampType::Input>(
        orders, outputs);
    std::cout << "Correct using output order correctness, time-priority, "
                 "orders sorted by input timestamp"
              << std::endl;
    return;
  } catch (const std::exception &exc) {
    errors.push_back(std::string{"output order, time priority, orders "
                                 "sorted by input timestamp error: "} +
                     exc.what());
  }

  // none of the correctness tests passed, throw error
  std::string exc_what = "checking correctness failed.\n";
  for (const auto &err : errors) {
    exc_what += '\n';
    exc_what += err;
  }
  throw std::runtime_error(exc_what);
}

GradingSession::~GradingSession() {
  if (!tempdir_path.empty() && !fork()) {
    execl("/bin/rm", "/bin/rm", "-rf", tempdir_path.c_str(), nullptr);
    _Exit(1);
  }
}
