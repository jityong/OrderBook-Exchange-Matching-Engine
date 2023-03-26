#include <algorithm>
#include <string>
#include <vector>

template <typename Seps>
inline std::vector<std::string> slow_split(std::string_view str,
                                           Seps&& seps) {
  std::vector<std::string> out;
  for (size_t start = 0; start < str.size(); ++start) {
    size_t len = 0;
    for (; len < str.size() - start; ++len) {
      if (std::find(std::begin(seps), std::end(seps), str[start + len]) !=
          std::end(seps)) {
        break;
      }
    }

    if (len > 0) {
      out.push_back(std::string{str.substr(start, len)});
    }
    start += len;
  }
  return out;
}
