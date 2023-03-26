#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <vector>

#include "grader.hpp"
#include "split.hpp"

// i hate C++
#define PARSE_INTO(string, number) \
  do {                             \
    istream.seekg(0);              \
    istream.str(string);           \
    istream >> number;             \
  } while (false)

static std::vector<size_t> parse_number_range(const std::string &range,
                                              size_t max) {
  std::istringstream istream;
  std::unordered_set<size_t> retv;
  for (std::string thread_set : slow_split(range, ",")) {
    auto range_spec = slow_split(thread_set, "-");
    if (range_spec.size() == 1) {
      size_t number;
      PARSE_INTO(range_spec[0], number);
      retv.insert(number);
    } else if (range_spec.size() == 2) {
      size_t start;
      size_t end;
      PARSE_INTO(range_spec[0], start);
      PARSE_INTO(range_spec[1], end);
        for (size_t i = start; i <= std::min(max, end);
             ++i) {
          retv.insert(i);
        }
    } else {
      assert(false);
    }
  }
  return {retv.begin(), retv.end()};
}

ParsedGraderInput parse_input_line(std::string &line,
                                   size_t num_threads) {
  std::istringstream istream;
  std::optional<std::vector<size_t> > threads = std::nullopt;
  GraderInput retv = {.type = GraderInputType::Invalid};

  auto tokens = slow_split(line, " \t");
  auto iter = tokens.begin();
  std::string command_token = *iter;
  if (std::isdigit(command_token[0])) {
    threads = parse_number_range(command_token, num_threads - 1);
    command_token = *(++iter);
  }
  assert(command_token.length() == 1);
  switch (command_token[0]) {
    case '.':
      retv.type = GraderInputType::Sync;
      break;
    case 'o':
      retv.type = GraderInputType::Connect;
      break;
    case 'x':
      retv.type = GraderInputType::Disconnect;
      break;
    case 's':
      retv.type = GraderInputType::Sleep;
      PARSE_INTO(*(++iter), retv.duration_ms);
      break;
    case 'w':
      retv.type = GraderInputType::Wait;
      PARSE_INTO(*(++iter), retv.order_id);
      break;
    case 'C': {
      retv.type = GraderInputType::SendCommand;
      retv.command = {.type = EngineCommandType::Cancel};
      PARSE_INTO(*(++iter), retv.command.order_id);
      break;
    }
    case 'B':
    case 'S': {
      retv.type = GraderInputType::SendCommand;
      retv.command = {.type = command_token[0] == 'S'
                                  ? EngineCommandType::Sell
                                  : EngineCommandType::Buy};
      PARSE_INTO(*(++iter), retv.command.order_id);
      {
        std::string symbol = *(++iter);
        assert(symbol.length() < sizeof(retv.command.instrument));
        strcpy(retv.command.instrument, symbol.c_str());
      }
      PARSE_INTO(*(++iter), retv.command.price);
      PARSE_INTO(*(++iter), retv.command.count);
      break;
    }
    default:
      assert(false);
      break;
  }
  return {threads, retv};
}

std::ostream &operator<<(std::ostream &os, const GraderInput &i) {
  switch (i.type) {
    case GraderInputType::Invalid:
      os << "Invalid";
      break;
    case GraderInputType::Connect:
      os << "Connect";
      break;
    case GraderInputType::Disconnect:
      os << "Disconnect";
      break;
    case GraderInputType::Sleep:
      os << "Sleep for " << i.duration_ms << " ms";
      break;
    case GraderInputType::Wait:
      os << "Wait for order " << i.order_id;
      break;
    case GraderInputType::SendCommand:
      os << "Send command: " << i.command;
      break;
    case GraderInputType::Sync:
      os << "Sync";
      break;
    default:
      assert(false);
      break;
  }
  return os;
}
