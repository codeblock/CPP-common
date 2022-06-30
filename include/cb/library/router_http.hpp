#ifndef CB_LIBRARY_ROUTER_HTTP_HPP_
#define CB_LIBRARY_ROUTER_HTTP_HPP_

#include <string>
#include <unordered_map>

#include "include/cb/common/types.h"

namespace cb {
namespace library {

class RouterHttp {
 public:
  //RouterHttp(void) {}
  //RouterHttp(const RouterHttp& rhs) {}
  //RouterHttp& operator = (const RouterHttp& rhs) {}

  typedef std::string(*method_t)(const ::cb::common::types::HttpRequest&);

  std::unordered_map<std::string, method_t>& routes(void) {
    return m_routes;
  }

  method_t& route(const std::string k) {
    return m_routes[k];
  }

 protected:
  std::unordered_map<std::string, method_t> m_routes;
};

} // namespace library
} // namespace cb

#endif