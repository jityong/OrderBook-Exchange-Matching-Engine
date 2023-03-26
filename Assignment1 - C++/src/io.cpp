// This file contains I/O functions.
// There should be no need to modify this file.

#include "io.h"

#include "engine.hpp"

extern "C" {
void *engine_new(void) { return static_cast<void *>(new Engine{}); }

void engine_accept(void *engine, void *file) {
  static_cast<Engine *>(engine)->Accept(ClientConnection{file});
}

int read_input(void *file, struct input *output);

struct _IO_FILE;
typedef struct _IO_FILE FILE;
int fclose(FILE *stream);
}

void ClientConnection::FreeHandle() {
  if (handle) {
    fclose(static_cast<FILE *>(handle));
    handle = nullptr;
  }
}

ReadResult ClientConnection::ReadInput(input &read_into) {
  switch (read_input(handle, &read_into)) {
    case 1:
      return ReadResult::EndOfFile;
    case 0:
      return ReadResult::Success;
    default:
      return ReadResult::Error;
  }
}
