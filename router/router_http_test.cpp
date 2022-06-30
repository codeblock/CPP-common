#include <string>

#include "include/cb/common/types.h"
#include "router/router_http_test.h"

RouterHttpTest::RouterHttpTest(void) {
  m_routes = {
    {"/a", [](const cb::common::types::HttpRequest& req) -> std::string { return std::string("a"); }},
    {"/b", [](const cb::common::types::HttpRequest& req) -> std::string { return std::string("b"); }},
    {"/c", [](const cb::common::types::HttpRequest& req) -> std::string { return std::string("c"); }}
  };
}