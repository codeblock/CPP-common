#ifndef CB_LIBRARY_SERVER_HTTP_BOOST_H_
#define CB_LIBRARY_SERVER_HTTP_BOOST_H_

#include <string>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

#include "include/cb/library/router_http.hpp"

namespace {

class ServerHttpBoostAcceptor;

} // namespace

namespace cb {
namespace library {

class ServerHttpBoost {
 public:
  ServerHttpBoost(unsigned short port_num, unsigned int thread_pool_size);
  virtual ~ServerHttpBoost();

  // Start the server.
  void start();
  // Stop the server.
  void stop();

  // Definition the services
  void setServiceStatic(const std::string service_static);
  void setServiceRouter(const RouterHttp& service_router);

 private:
  unsigned short m_port_num;
  unsigned int m_thread_pool_size;

  boost::asio::io_service m_ios;
  std::unique_ptr<boost::asio::io_service::work> m_work;
  std::unique_ptr<::ServerHttpBoostAcceptor> acc;
  std::vector<std::unique_ptr<std::thread>> m_thread_pool;
};

} // namespace library
} // namespace cb

#endif
