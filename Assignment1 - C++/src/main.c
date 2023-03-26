// This file contains main() as well as the logic setting up the I/O.
// There should be no need to modify this file.

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "io.h"

void *engine_new(void);
void engine_accept(void *engine, void *file);

int read_input(void *file, struct input *output) {
  if (fread_unlocked(output, 1, sizeof(*output), file) !=
      sizeof(*output)) {
    return feof(file) ? 1 : -1;
  }
  return 0;
}

static int listenfd = -1;
static char *socketpath = NULL;

static void handle_exit_signal(int signum) {
  (void)signum;
  exit(0);
}

static void exit_cleanup(void) {
  if (listenfd == -1) {
    return;
  }

  close(listenfd);

  if (socketpath) {
    unlink(socketpath);
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <socket path>\n", argv[0]);
    return 1;
  }

  socketpath = argv[1];
  listenfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listenfd == -1) {
    perror("socket");
    return 1;
  }

  {
    struct sockaddr_un sockaddr = {.sun_family = AF_UNIX};
    strncpy(sockaddr.sun_path, argv[1], sizeof(sockaddr.sun_path) - 1);
    if (bind(listenfd, &sockaddr, sizeof(sockaddr)) != 0) {
      perror("bind");
      return 1;
    }
  }

  atexit(exit_cleanup);
  signal(SIGINT, handle_exit_signal);
  signal(SIGTERM, handle_exit_signal);

  if (listen(listenfd, 8) != 0) {
    perror("listen");
    return 1;
  }

  void *engine = engine_new();
  if (!engine) {
    fprintf(stderr, "Failed to allocate Engine\n");
    return 1;
  }

  while (1) {
    int connfd = accept(listenfd, NULL, NULL);
    if (connfd == -1) {
      perror("accept");
      return 1;
    }
    FILE *conn = fdopen(connfd, "r+");
    setvbuf(conn, NULL, _IOFBF, BUFSIZ);
    engine_accept(engine, conn);
  }

  return 0;
}
