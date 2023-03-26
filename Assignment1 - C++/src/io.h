// This file contains definitions used by the provided I/O code.
// There should be no need to modify this file.

#ifndef IO_H
#define IO_H

#ifdef __cplusplus
#include <cstdint>
#include <iostream>
#include <sstream>

extern "C" {
#else
#include <stdint.h>
#endif

enum input_type { input_buy = 'B', input_sell = 'S', input_cancel = 'C' };

struct input {
  enum input_type type;
  uint32_t order_id;
  uint32_t price;
  uint32_t count;
  char instrument[9];
};

#ifdef __cplusplus
}

enum class ReadResult { Success, EndOfFile, Error };

class ClientConnection {
  void* handle;
  void FreeHandle();

 public:
  inline explicit ClientConnection(void* handle) : handle(handle) {}
  ClientConnection(const ClientConnection&) = delete;
  ClientConnection& operator=(const ClientConnection&) = delete;
  inline ClientConnection(ClientConnection&& other) : handle(nullptr) {
    handle = other.handle;
    other.handle = nullptr;
  }

  inline ClientConnection& operator=(ClientConnection&& other) {
    FreeHandle();
    handle = other.handle;
    other.handle = nullptr;

    return *this;
  }

  inline ~ClientConnection() { FreeHandle(); }

  ReadResult ReadInput(input& read_into);
};

class Output {
 public:
  inline static void OrderAdded(uint32_t id, const char* symbol,
                                uint32_t price, uint32_t count,
                                bool is_sell_side,
                                intmax_t input_timestamp,
                                intmax_t output_timestamp) {
      std::stringstream msg;
      msg << (is_sell_side ? "S" : "B") << " " << id << " " << symbol
          << " " << price << " " << count << " " << input_timestamp
          << " " << output_timestamp << "\n";
//      std::cout << (is_sell_side ? "S" : "B") << " " << id << " " << symbol
//              << " " << price << " " << count << " " << input_timestamp
//              << " " << output_timestamp << std::endl;
        std::cout << msg.str() << std::flush;
  }

  inline static void OrderExecuted(uint32_t resting_id, uint32_t new_id,
                                   uint32_t execution_id, uint32_t price,
                                   uint32_t count,
                                   intmax_t input_timestamp,
                                   intmax_t output_timestamp) {
      std::stringstream msg;
      msg << "E " << resting_id << " " << new_id << " "
             << execution_id << " " << price << " " << count << " "
             << input_timestamp << " " << output_timestamp << "\n";
//    std::cout << "E " << resting_id << " " << new_id << " "
//              << execution_id << " " << price << " " << count << " "
//              << input_timestamp << " " << output_timestamp << std::endl;
        std::cout << msg.str() << std::flush;
  }

  inline static void OrderDeleted(uint32_t id, bool cancel_accepted,
                                  intmax_t input_timestamp,
                                  intmax_t output_timestamp) {
      std::stringstream msg;
      msg << "X " << id << " " << (cancel_accepted ? "A" : "R") << " "
                << input_timestamp << " " << output_timestamp << "\n";
//    std::cout << "X " << id << " " << (cancel_accepted ? "A" : "R") << " "
//              << input_timestamp << " " << output_timestamp << std::endl;
      std::cout << msg.str() << std::flush;
  }
};
#endif

#endif
