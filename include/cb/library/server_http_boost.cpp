/**
 * 
 * @reference
 * Dmytro Radchuk - Boost.Asio C++ Network Programming Cookbook
 */

#include <cassert>

#include <atomic>
#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <boost/filesystem.hpp>

#include "include/cb/common/defines.h"
#include "include/cb/common/types.h"
#include "include/cb/common/utils.hpp"
#include "include/cb/common/logger.h"
#include "include/cb/library/server_http_boost.h"

namespace {

uintmax_t max_res_filesize = 1024 * 1024 * 10;
std::string service_static;
::cb::library::RouterHttp service_router;

// processor
class ServerHttpBoostService {
  static const std::map<unsigned int, std::string> http_status_table;
  static const std::string delim_line_each;
  static const std::string delim_line_section;

 public:
  ServerHttpBoostService(std::shared_ptr<boost::asio::ip::tcp::socket> sock);
  virtual ~ServerHttpBoostService();

  void start_handling();
  void lock();
  void unlock();

 private:
  void on_request_line_received(const boost::system::error_code& ec, std::size_t bytes_transferred);
  void on_headers_received(const boost::system::error_code& ec, std::size_t bytes_transferred);
  bool process_request_router();
  bool process_request_static();
  void send_response();
  void on_response_sent(const boost::system::error_code& ec, std::size_t bytes_transferred);
  void on_finish();

 private:
  std::shared_ptr<boost::asio::ip::tcp::socket> m_sock;
  boost::asio::streambuf m_request;
  std::map<std::string, std::string> m_request_headers;
  std::string m_requested_resource, m_requested_query_string;
  std::unique_ptr<char[]> m_resource_buffer;
  unsigned int m_response_status_code;
  std::size_t m_resource_size_bytes;
  std::string m_response_headers;
  std::string m_response_status_line;

  bool m_recv;

  ::cb::common::types::HttpRequest m_req;
};

const std::map<unsigned int, std::string> ServerHttpBoostService::http_status_table = {
  { 200, "200 OK" },
  { 400, "400 Bad Request" },
  { 403, "403 Forbidden" },
  { 404, "404 Not Found" },
  { 408, "408 Request Timeout" },
  { 413, "413 Request Entity Too Large" },
  { 500, "500 Server Error" },
  { 501, "501 Not Implemented" },
  { 504, "504 Gateway Timeout" },
  { 505, "505 HTTP Version Not Supported" }
};

const std::string ServerHttpBoostService::delim_line_each = "\r\n";
const std::string ServerHttpBoostService::delim_line_section = "\r\n\r\n";

// receiver
class ServerHttpBoostAcceptor {
 public:
  ServerHttpBoostAcceptor(boost::asio::io_service& ios, unsigned short port_num);
  virtual ~ServerHttpBoostAcceptor();

  void start();
  void stop();

 private:
  void initAccept();
  void onAccept(const boost::system::error_code& ec, std::shared_ptr<boost::asio::ip::tcp::socket> sock);

 private:
  boost::asio::io_service& m_ios;
  boost::asio::ip::tcp::acceptor m_acceptor;
  std::atomic<bool> m_isStopped;
};

} // namespace

// ---------------------------------------------------- ServerHttpBoostAcceptor
::ServerHttpBoostAcceptor::ServerHttpBoostAcceptor(boost::asio::io_service& ios, unsigned short port_num) :
  m_ios(ios),
  m_acceptor(m_ios,
    boost::asio::ip::tcp::endpoint(
      boost::asio::ip::address_v4::any(),
      port_num)),
  m_isStopped(false)
{}

::ServerHttpBoostAcceptor::~ServerHttpBoostAcceptor()
{}

// Start accepting incoming connection requests.
void ::ServerHttpBoostAcceptor::start() {
  m_acceptor.listen();
  initAccept();
}

// Stop accepting incoming connection requests.
void ::ServerHttpBoostAcceptor::stop() {
  m_isStopped.store(true);
}

void ::ServerHttpBoostAcceptor::initAccept() {
  std::shared_ptr<boost::asio::ip::tcp::socket> sock(new boost::asio::ip::tcp::socket(m_ios));
  //m_acceptor.set_option(boost::asio::socket_base::keep_alive(true));
  m_acceptor.async_accept(*sock.get(), [this, sock](const boost::system::error_code& error) {
    onAccept(error, sock);
  });
}

void ::ServerHttpBoostAcceptor::onAccept(const boost::system::error_code& ec, std::shared_ptr<boost::asio::ip::tcp::socket> sock) {
  if (ec == boost::system::errc::success) {
    //boost::asio::socket_base::keep_alive option(true);
    //sock->set_option(option);
    (new ::ServerHttpBoostService(sock))->start_handling();
  } else {
    std::ostringstream oss;
    oss << __FUNCTION__ << ":" << __LINE__ << ": " << "Error occured! Error code = " << ec.value() << ". Message: " << ec.message();
    ::cb::common::Logger::log(::cb::common::Logger::eLevel::eError, "%s", oss.str().c_str());

    return;
  }

  // Init next async accept operation if
  // acceptor has not been stopped yet.
  if (!m_isStopped.load()) {
    initAccept();
  } else {
    // Stop accepting incoming connections
    // and free allocated resources.
    m_acceptor.close();
  }
}

// ---------------------------------------------------- ServerHttpBoostService
::ServerHttpBoostService::ServerHttpBoostService(std::shared_ptr<boost::asio::ip::tcp::socket> sock) :
  m_sock(sock),
  m_request(4096),
  m_response_status_code(200), // Assume success.
  m_resource_size_bytes(0),
  m_recv(false)
{
  // for remote_endpoint: Transport endpoint is not connected
  boost::system::error_code errcode;
  boost::asio::ip::tcp::endpoint endpoint = m_sock.get()->remote_endpoint(errcode);

  if (errcode == boost::system::errc::success) {
    m_req.remote_addr = endpoint.address().to_string();
    m_req.remote_port = endpoint.port();
  }
}

::ServerHttpBoostService::~ServerHttpBoostService()
{}

void ::ServerHttpBoostService::start_handling() {
  boost::system::error_code errcode;
  boost::asio::ip::tcp::endpoint endpoint = m_sock.get()->remote_endpoint(errcode);

  if (errcode != boost::system::errc::success) {
    // remote_endpoint: Transport endpoint is not connected
    return on_finish();
  }

  boost::asio::async_read_until(*m_sock.get(), m_request, delim_line_each, [this](const boost::system::error_code& ec, std::size_t bytes_transferred) {
    on_request_line_received(ec, bytes_transferred);
  });
}

void ::ServerHttpBoostService::on_request_line_received(const boost::system::error_code& ec, std::size_t bytes_transferred) {
  std::ostringstream oss;

  if (ec != boost::system::errc::success) {
    oss << __FUNCTION__ << ":" << __LINE__ << ": " << "Error occured! Error code = " << ec.value() << ". Message: " << ec.message();
    ::cb::common::Logger::log(::cb::common::Logger::eLevel::eError, "%s", oss.str().c_str());

    if (ec == boost::asio::error::not_found) {
      // No delimiter has been found in the
      // request message.
      m_response_status_code = 413;
      send_response();

      return;
    } else {
      // In case of any other error –
      // close the socket and clean up.
      on_finish();

      return;
    }
  }

  //auto bufs = m_request.data();
  //std::string result(buffers_begin(bufs), buffers_begin(bufs) + m_request.size());
  //oss.str(""); oss.clear();
  //oss << "\n---------------------------- base info\n" << result << "---------------------------- base info";
  //::cb::common::Logger::log(::cb::common::Logger::eLevel::eDebug, "%s", oss.str().c_str());

  // Parse the request line.
  std::string request_line;
  std::istream request_stream(&m_request);
  std::getline(request_stream, request_line, '\r');
  // Remove symbol '\n' from the buffer.
  request_stream.get();
  // Parse the request line.
  std::istringstream request_line_stream(request_line);
  request_line_stream >> m_req.method;

  request_line_stream >> m_requested_resource;

  std::string request_http_version;
  request_line_stream >> request_http_version;

  // We only support GET, POST method.
  if (m_req.method.compare("GET") != 0 && m_req.method.compare("POST") != 0) {
    // Unsupported method.
    m_response_status_code = 501;
    send_response();

    return;
  }

  if (request_http_version.compare("HTTP/1.1") != 0) {
    // Unsupported HTTP version or bad request.
    m_response_status_code = 505;
    send_response();

    return;
  }

  // At this point the request line is successfully
  // received and parsed. Now read the request headers.
  boost::asio::async_read_until(*m_sock.get(), m_request, delim_line_section, [this](const boost::system::error_code& ec, std::size_t bytes_transferred) {
    on_headers_received(ec, bytes_transferred);
  });

  return;
}

void ::ServerHttpBoostService::on_headers_received(const boost::system::error_code& ec, std::size_t bytes_transferred) {
  std::ostringstream oss;

  if (ec != boost::system::errc::success) {
    oss << __FUNCTION__ << ":" << __LINE__ << ": " << "Error occured! Error code = " << ec.value() << ". Message: " << ec.message();
    ::cb::common::Logger::log(::cb::common::Logger::eLevel::eError, "%s", oss.str().c_str());

    if (ec == boost::asio::error::not_found) {
      // No delimiter has been fonud in the
      // request message.
      m_response_status_code = 413;
      send_response();

      return;
    } else {
      // In case of any other error - close the
      // socket and clean up.
      on_finish();

      return;
    }
  }

  std::size_t isquery = m_requested_resource.find('?');
  if (isquery == std::string::npos) {
    m_req.path = m_requested_resource;
  } else {
    m_req.path = m_requested_resource.substr(0, isquery);
  }

  if (m_req.method == "GET") {
    if (isquery != std::string::npos) {
      m_requested_query_string = m_requested_resource.substr(isquery + 1);
    }
  } else {
    auto param = m_request.data();
    std::string result(buffers_begin(param), buffers_begin(param) + m_request.size());
    std::size_t params_start = result.find(delim_line_section);
    if (params_start != std::string::npos) {
      m_requested_query_string = result.substr(params_start + delim_line_section.size());
    }
  }

  oss.str(""); oss.clear();
  oss << "recv: {\"method\": \"" << m_req.method << "\", \"path\": \"" << m_req.path << "\", \"params\": \"" << m_requested_query_string << "\"}";
  ::cb::common::Logger::log(::cb::common::Logger::eLevel::eInfo, "%s", oss.str().c_str());

  if (m_requested_query_string.empty() == false) {
    std::string k, v;
    std::size_t t;
    std::istringstream iss(m_requested_query_string);
    for (std::string token; std::getline(iss, token, '&'); ) {

      v = std::move(token);
      t = std::min<std::size_t>(v.find("="), v.size());
      if (t == 0) {
        t = v.size();
      }
      k = v.substr(0, t);

      if (k.empty() == false) {
        t = std::min<std::size_t>(t + 1, v.size());
        v = v.substr(t);
        m_req.params[k] = v;
      }
    }
  }

  // static first
  if (process_request_static() == false || m_response_status_code != 200) {
    // router second
    process_request_router();
  }
  send_response();
}

bool ::ServerHttpBoostService::process_request_router() {
  bool rtn = false;

  std::ostringstream oss;
  std::string res;

  if (service_router.routes().find(m_req.path) == service_router.routes().end()) {
    m_response_status_code = 404;

    oss.str(""); oss.clear();
    oss << "doesn't exist key in map: " << m_req.path;
    ::cb::common::Logger::log(::cb::common::Logger::eLevel::eWarn, "%s", oss.str().c_str());

    return rtn;
  } else {
    try {
      m_response_status_code = 200;

      res = service_router.route(m_req.path)(m_req);
      rtn = true;
    } catch (std::exception& err) {
      m_response_status_code = 500;

      oss.str(""); oss.clear();
      oss << __FUNCTION__ << ":" << __LINE__ << ": " << err.what();
      ::cb::common::Logger::log(::cb::common::Logger::eLevel::eError, "%s", oss.str().c_str());

      return rtn;
    }
  }

  m_resource_size_bytes = static_cast<std::size_t>(res.size());
  char* c_str = new char[m_resource_size_bytes + 1];
  std::copy(res.begin(), res.end(), c_str);
  c_str[m_resource_size_bytes] = '\0';
  m_resource_buffer.reset(c_str);

  ::cb::common::Logger::log(::cb::common::Logger::eLevel::eInfo, "send: %s", c_str);

  //std::ifstream::{app, ate, binary, in, out, trunc}

  return rtn;
}

bool ::ServerHttpBoostService::process_request_static() {
  bool rtn = false;

  if (service_static.length() == 0 || !boost::filesystem::exists(service_static)) {
    return rtn;
  }

  rtn = true;

  // Read file.
  std::string resource_file_path = std::string(service_static) + m_req.path;
  ::cb::common::utils::stringReplace(resource_file_path, std::string(1, '/'), std::string(1, PATH_SEP), true);
  if (!boost::filesystem::exists(resource_file_path)) {
    // Resource not found.
    m_response_status_code = 404;

    return rtn;
  }

  std::ifstream resource_fstream(resource_file_path, std::ifstream::binary);
  if (!resource_fstream.is_open()) {
    // Could not open file.
    // Something bad has happened.
    m_response_status_code = 500;

    return rtn;
  }

  // limit filesize
  if (boost::filesystem::file_size(resource_file_path) > max_res_filesize) {
    m_response_status_code = 504;

    return rtn;
  }

  // Find out file size.
  resource_fstream.seekg(0, std::ifstream::end);
  m_resource_size_bytes = static_cast<std::size_t>(resource_fstream.tellg());
  m_resource_buffer.reset(new char[m_resource_size_bytes]);
  resource_fstream.seekg(std::ifstream::beg);
  resource_fstream.read(m_resource_buffer.get(), m_resource_size_bytes);

  ::cb::common::Logger::log(::cb::common::Logger::eLevel::eInfo, "send: %s", m_req.path.c_str());

  return rtn;
}

void ::ServerHttpBoostService::send_response() {
  std::ostringstream oss;

  try {
    m_sock->shutdown(boost::asio::ip::tcp::socket::shutdown_receive);
  } catch (std::exception& err) {
    // Transport endpoint is not connected
    oss.str(""); oss.clear();
    oss << __FUNCTION__ << ":" << __LINE__ << ": " << err.what();
    ::cb::common::Logger::log(::cb::common::Logger::eLevel::eError, "%s", oss.str().c_str());

    return on_finish();
  }

  m_response_headers += std::string("Connection: close").append(delim_line_each);
  m_response_headers += std::string("Content-Type: text/html; charset=utf-8").append(delim_line_each);
  m_response_headers += std::string("Content-Length: ") + std::to_string(m_resource_size_bytes).append(delim_line_each);
  m_response_headers += std::string("Server: Boost.Asio").append(delim_line_each);

  // auto status_line = http_status_table.at(m_response_status_code);
  const std::string status_line = http_status_table.at(m_response_status_code);
  m_response_status_line = std::string("HTTP/1.1 ") + status_line + delim_line_each;
  m_response_headers += delim_line_each;
  std::vector<boost::asio::const_buffer> response_buffers;
  response_buffers.push_back(boost::asio::buffer(m_response_status_line));

  if (m_response_headers.length() > 0) {
    response_buffers.push_back(
      boost::asio::buffer(m_response_headers));
  }

  if (m_resource_size_bytes > 0) {
    response_buffers.push_back(boost::asio::buffer(m_resource_buffer.get(), m_resource_size_bytes));
  }

  // Initiate asynchronous write operation.
  boost::asio::async_write(*m_sock.get(), response_buffers, [this](const boost::system::error_code& ec, std::size_t bytes_transferred) {
    on_response_sent(ec, bytes_transferred);
  });
}

void ::ServerHttpBoostService::on_response_sent(const boost::system::error_code& ec, std::size_t bytes_transferred) {
  std::ostringstream oss;

  if (ec != boost::system::errc::success) {
    oss << __FUNCTION__ << ":" << __LINE__ << ": " << "Error occured! Error code = " << ec.value() << ". Message: " << ec.message();
    ::cb::common::Logger::log(::cb::common::Logger::eLevel::eError, "%s", oss.str().c_str());
  }

  boost::system::error_code errcode;
  boost::asio::ip::tcp::endpoint endpoint = m_sock->remote_endpoint(errcode);
  if (errcode == boost::system::errc::success) {
    try {
      m_sock->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
    } catch (std::exception& err) {
      // Transport endpoint is not connected
      oss.str(""); oss.clear();
      oss << __FUNCTION__ << ":" << __LINE__ << ": " << err.what();
      ::cb::common::Logger::log(::cb::common::Logger::eLevel::eError, "%s", oss.str().c_str());
    }
  } else {
    // Transport endpoint is not connected (client disconnected already)
    oss.str(""); oss.clear();
    oss << "Transport endpoint is not connected - ec: " << errcode;
    ::cb::common::Logger::log(::cb::common::Logger::eLevel::eDebug, "%s", oss.str().c_str());
  }

  on_finish();
}

// Here we perform the cleanup.
void ::ServerHttpBoostService::on_finish() {
  delete this;
}

//} // namespace

namespace cb {
namespace library {

ServerHttpBoost::ServerHttpBoost(unsigned short port_num, unsigned int thread_pool_size)
{
  m_port_num = port_num;
  m_thread_pool_size = thread_pool_size;
  m_work.reset(new boost::asio::io_service::work(m_ios));
}

ServerHttpBoost::~ServerHttpBoost()
{}

// Start the server.
void ServerHttpBoost::start() {
  assert(m_thread_pool_size > 0);
  // Create and start Acceptor.
  acc.reset(new ::ServerHttpBoostAcceptor(m_ios, m_port_num));
  acc->start();
  // Create specified number of threads and
  // add them to the pool.
  for (unsigned int i = 0; i < m_thread_pool_size; i++) {
    std::unique_ptr<std::thread> th(
      new std::thread([this]() {
        m_ios.run();
      })
    );
    m_thread_pool.push_back(std::move(th));
  }

  cb::common::Logger::log(cb::common::Logger::eLevel::eInfo, "Server Started on port %hu with %u threads", m_port_num, m_thread_pool_size);
}

// Stop the server.
void ServerHttpBoost::stop() {
  acc->stop();
  m_ios.stop();
  for (auto& th : m_thread_pool) {
    th->join();
  }
}

void ServerHttpBoost::setServiceStatic(const std::string service_static) {
  if (::service_static.length() == 0) {
    ::service_static = service_static;
  }
}

void ServerHttpBoost::setServiceRouter(const RouterHttp& service_router) {
  if (::service_router.routes().size() == 0) {
    ::service_router = service_router;
  }
}

} // namespace library
} // namespace cb