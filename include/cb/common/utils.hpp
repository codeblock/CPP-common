#ifndef CB_COMMON_UTILS_HPP_
#define CB_COMMON_UTILS_HPP_

#include <string>

namespace cb {
namespace common {
namespace utils {
 
void stringReplace(std::string& subject, const std::string& search, const std::string& replace, const bool recursive = false) {
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos)
  {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
    if (recursive == false) {
      break;
    }
  }
}

} // namespace utils
} // namespace common
} // namespace cb

#endif
