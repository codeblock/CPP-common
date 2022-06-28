#ifndef CB_COMMON_TYPES_H_
#define CB_COMMON_TYPES_H_

#include <string>
#include <unordered_map>

namespace cb {
namespace common {
namespace types {

typedef struct {
  std::string method;
  std::string path;
  std::string remote_addr;
  unsigned short remote_port;
  std::unordered_map<std::string, std::string> params;
} HttpRequest;

} // namespace types
} // namespace common
} // namespace cb

#endif