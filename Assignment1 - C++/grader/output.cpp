#include <concepts>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "grader.hpp"
#include "split.hpp"

// i hate C++
// same tbh
#define PARSE_INTO(string, number) \
  do {                             \
    istream.seekg(0);              \
    istream.str(string);           \
    istream >> number;             \
  } while (false)

ParsedEngineOutput parse_engine_output_line(std::string_view line) {
  auto parse_fail = [](bool condition, const char* fail_reason) {
    if (!condition) {
      throw std::runtime_error(fail_reason);
    }
  };

  auto parse_into_int = [](const std::string& str, auto& dest,
                           const char* fail_reason) {
    try {
      if constexpr (std::same_as<std::decay_t<decltype(dest)>, int>) {
        dest = std::stoi(str, nullptr, 10);
      } else if constexpr (std::same_as<std::decay_t<decltype(dest)>,
                                        long>) {
        dest = std::stol(str, nullptr, 10);
      } else if constexpr (std::same_as<std::decay_t<decltype(dest)>,
                                        unsigned long>) {
        dest = std::stoul(str, nullptr, 10);
      } else if constexpr (std::same_as<std::decay_t<decltype(dest)>,
                                        long long>) {
        dest = std::stoll(str, nullptr, 10);
      } else if constexpr (std::same_as<std::decay_t<decltype(dest)>,
                                        unsigned long long>) {
        dest = std::stoull(str, nullptr, 10);
      } else {
        auto tmp = std::stoul(str, nullptr, 10);
        if (tmp > std::numeric_limits<unsigned>::max()) {
          throw std::out_of_range(str + " does not fit in unsigned");
        }
        dest = tmp;
      }
    } catch (const std::invalid_argument&) {
      throw std::runtime_error(fail_reason +
                               std::string{": invalid argument"});
    } catch (const std::out_of_range&) {
      throw std::runtime_error(fail_reason +
                               std::string{": out of range"});
    }
  };

  auto parse_into_char_array = [parse_fail](const std::string& str,
                                            auto& dest,
                                            const char* fail_reason) {
    parse_fail(str.length() < sizeof(dest), fail_reason);
    strcpy(dest, str.c_str());
  };

  auto parse_into_timestamp = [parse_into_int](const std::string& str,
                                               uintmax_t& dest,
                                               const char* fail_reason) {
    if (str.ends_with("ns")) {
      parse_into_int(str.substr(0, str.size() - 2), dest, fail_reason);
    } else if (str.ends_with("us")) {
      intmax_t tmp;
      parse_into_int(str.substr(0, str.size() - 2), tmp, fail_reason);
      dest = tmp * 1000;
      if (dest / 1000 != tmp) {
        throw std::runtime_error(fail_reason +
                                 std::string{": out of range"});
      }
    } else if (str.ends_with("ms")) {
      intmax_t tmp;
      parse_into_int(str.substr(0, str.size() - 2), tmp, fail_reason);
      dest = tmp * 1000000;
      if (dest / 1000000 != tmp) {
        throw std::runtime_error(fail_reason +
                                 std::string{": out of range"});
      }
    } else {
      // microseconds by default
      intmax_t tmp;
      parse_into_int(str, tmp, fail_reason);
      dest = tmp * 1000;
      if (dest / 1000 != tmp) {
        throw std::runtime_error(fail_reason +
                                 std::string{": out of range"});
      }
    }
  };

  std::istringstream istream;
  ParsedEngineOutput retv = {.type = EngineOutputType::Invalid,
                             .line = std::string{line}};

  auto tokens = slow_split(line, " \t");
  auto iter = tokens.begin();

  auto next = [end = tokens.end()](auto& iter) {
    if (++iter == end) {
      throw std::runtime_error("expected token");
    } else {
      return iter;
    }
  };

  std::string command_token = *iter;
  parse_fail(command_token.length() == 1, "output type too long");
  switch (command_token[0]) {
    case (char)EngineOutputType::Buy:
    case (char)EngineOutputType::Sell: {
      retv.type = command_token[0] == (char)EngineOutputType::Buy
                      ? EngineOutputType::Buy
                      : EngineOutputType::Sell;
      // S 123 GOOG 2700 10
      parse_into_int(*next(iter), retv.buysell.order_id,
                     "expected integer order id");
      parse_into_char_array(*next(iter), retv.buysell.instrument,
                            "symbol too long");
      parse_into_int(*next(iter), retv.buysell.price,
                     "expected integer price");
      parse_into_int(*next(iter), retv.buysell.count,
                     "expected integer count");
      break;
    }

    case (char)EngineOutputType::Exec: {
      retv.type = EngineOutputType::Exec;
      // E 123 125 1 2700 10
      parse_into_int(*next(iter), retv.exec.resting_order_id,
                     "expected integer resting order id");
      parse_into_int(*next(iter), retv.exec.new_order_id,
                     "expected integer new order id");
      parse_into_int(*next(iter), retv.exec.execution_id,
                     "expected integer execution id");
      if (!std::isdigit((*++iter)[0])) {
        ++iter;
      }
      parse_into_int(*(iter), retv.exec.price, "expected integer price");
      parse_into_int(*next(iter), retv.exec.count,
                     "expected integer count");
      break;
    }

    case (char)EngineOutputType::Cancel: {
      retv.type = EngineOutputType::Cancel;
      // X 125 A
      parse_into_int(*next(iter), retv.cancel.order_id,
                     "expected integer order id");
      {
        std::string cancel_type_token = *next(iter);
        parse_fail(cancel_type_token.length() == 1,
                   "cancel type too long");
        if (cancel_type_token[0] == (char)CancelType::Accept) {
          retv.cancel.cancel_type = CancelType::Accept;
        } else if (cancel_type_token[0] == (char)CancelType::Reject) {
          retv.cancel.cancel_type = CancelType::Reject;
        } else {
          parse_fail(false, "invalid cancel type");
        }
      }
      break;
    }

    default:
      parse_fail(false, "invalid output type");
      break;
  }

  // Parse timestamp
  parse_into_timestamp(*next(iter), retv.input_timestamp,
                       "expected integer input timestamp");
  parse_into_timestamp(*next(iter), retv.output_timestamp,
                       "expected integer output timestamp");

  parse_fail(++iter == tokens.end(), "extra tokens at end of output");

  return retv;
}
