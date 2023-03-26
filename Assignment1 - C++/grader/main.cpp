#include <csignal>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

#include "grader.hpp"

int main(int argc, char *argv[]) {
  try {
    if (argc < 2) {
      SyncCerr{} << "Usage: " << argv[0] << " <path to binary>"
                 << std::endl;
      return 1;
    }

    {
      // i hate C++
      // C: sigaction(SIGCHLD, &(struct sigaction){ .sa_handler = SIG_DFL,
      // .sa_flags = SA_NOCLDWAIT }, NULL);
      struct sigaction act {};
      act.sa_handler = SIG_DFL;
      act.sa_flags = SA_NOCLDWAIT;
      sigaction(SIGCHLD, &act, nullptr);
    }

    std::optional<size_t> threads = std::nullopt;
    std::vector<ParsedGraderInput> parsed_input;

    for (std::string input_line;
         std::ws(std::cin), std::getline(std::cin, input_line);) {
      if (input_line.starts_with("#") || input_line.empty()) {
        continue;
      }

      if (!threads.has_value()) {
        size_t read_threads;
        (std::istringstream(input_line)) >> read_threads;
        threads = read_threads;
        continue;
      }

      parsed_input.push_back(
          parse_input_line(input_line, threads.value()));
#if 0
      auto parsed = parse_input_line(input_line);
      if (parsed.first.has_value()) {
        for (size_t i : parsed.first.value()) {
          std::cout << i << " ";
        }
      }
      std::cout << parsed.second << std::endl;
#endif
    }

    if (std::cin.bad()) {
      SyncCerr{} << "Error reading input" << std::endl;
      return 1;
    }

    GradingSession::run(threads.value(), std::move(parsed_input),
                        argv[1]);
    return 0;
  } catch (std::exception &error) {
    SyncCerr{} << "Caught exception: " << error.what() << std::endl;
  }
  return 1;
}
