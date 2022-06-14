#ifndef CB_POOL_HPP_
#define CB_POOL_HPP_

#include <ctime>
#include <vector>
#include <functional> // hash
#include <mutex>

#include "include/cb/defines.h"
#include "include/cb/logger.h"

namespace cb {

class IPoolResource
{
public:
  virtual bool connect() = 0;
  virtual bool disconnect() = 0;
};

template <typename T>
struct tagPoolBody
{
  T* body; // class T : public IPoolResource
  int timeout;
  time_t past;
  short use;
};

template <typename T> // class T : public IPoolResource
class Pool
{
public:
  Pool(void);
  ~Pool(void);

  void set(unsigned int n);
  T* get(void);
  void release(T* rsc);
  void clear(void);
private:
  Pool(const Pool& rhw);
  Pool& operator =(const Pool& rhw);
  void* operator new (std::size_t) throw (std::bad_alloc); // prevent dynamic allocation
private:
  std::mutex m_mtx;
  int m_size;
  int m_wait;
  std::vector<tagPoolBody<T>> m_vec;
  std::hash<T*> m_hash;
};

template <typename T>
Pool<T>::Pool(void) : m_size(0), m_wait(0)
{
}

template <typename T>
Pool<T>::~Pool(void)
{
  //clear();
}

template <typename T>
void Pool<T>::set(unsigned int n)
{
  short s = 0;

  if (m_size == 0)
  {
    m_mtx.lock();
    if (m_size == 0)
    {
      m_size = n;
      m_wait = 1;//m_size << 1;
      m_vec.reserve(m_size);
      for (s = 0; s < m_size; ++s)
      {
        tagPoolBody<T> ps;
        ps.body = new T();
        //ps.body->connect(); // not yet set the connection informations
        ps.timeout = 28800 - 1;
        ps.past = time(NULL);
        ps.use = 0;
        m_vec.push_back(ps);

        cb::Logger::log(cb::Logger::eLevel::eInfo, "%s%hd", "Pool set #", (s + 1));
      }
    }
    m_mtx.unlock();
  }
}

template <typename T>
T* Pool<T>::get(void)
{
  typename std::vector<tagPoolBody<T>>::iterator it = m_vec.begin();

  short s = 0;
  T* rsc = NULL;
  time_t past;

  if (m_size > 0)
  {
    past = time(NULL);
    while (rsc == NULL)
    {
      s = 0;
      //printf("(%d / %d)\n", time(NULL) - past, m_wait);
      for (auto it = m_vec.begin(); it != m_vec.end(); ++it)
      {
        s++;
        if (it->use == 0)
        {
          m_mtx.lock();
          if (it->use == 0)
          {
            if (time(NULL) - it->past > it->timeout)
            {
              cb::Logger::log(cb::Logger::eLevel::eDebug, "%s%hd%s%d%s%d%s", "Pool refresh #", s, " for timeout (", (time(NULL) - it->past), " / ", it->timeout, " sec)");
              it->body->disconnect();
              it->body->connect();
            }
            rsc = it->body;
            it->past = time(NULL);
            it->use = 1;

            cb::Logger::log(cb::Logger::eLevel::eInfo, "%s%hd", "Pool get #", s);
          }
          m_mtx.unlock();

          if (it->use == 1)
          {
            break;
          }
        }
      }

      if (rsc == NULL) {
        int wait = 1;
        if (time(NULL) - past > m_wait) {
          wait = m_wait;
          past = time(NULL);
        }
        cb::Logger::log(cb::Logger::eLevel::eDebug, "%s%d%s", " sleep ................................................. ", wait, " sec");
        WAIT_A_SECONDS(wait);
      }
    }
  }

  return rsc;
}

template <typename T>
void Pool<T>::release(T* rsc)
{
  typename std::vector<tagPoolBody<T>>::iterator it = m_vec.begin();

  short s = 0;

  if (m_size > 0)
  {
    for (auto it = m_vec.begin(); it != m_vec.end(); ++it)
    {
      s++;
      if (it->use == 1 && m_hash(it->body) == m_hash(rsc))
      {
        it->use = 0;

        cb::Logger::log(cb::Logger::eLevel::eInfo, "%s%d", "Pool release #", s);
        break;
      }
    }
  }
}

template <typename T>
void Pool<T>::clear(void)
{
  typename std::vector<tagPoolBody<T>>::iterator it = m_vec.begin();

  short s = 0;

  if (m_size > 0)
  {
    m_mtx.lock();
    if (m_size > 0)
    {
      for (auto it = m_vec.begin(); it != m_vec.end(); ++it)
      {
        s++;
        it->body->disconnect();
        delete it->body; // warning: deleting object of polymorphic class type ‘PoolMySQL’ which has non-virtual destructor might cause undefined behaviour [-Wdelete-non-virtual-dtor]
        it->use = 0;

        cb::Logger::log(cb::Logger::eLevel::eInfo, "%s%d", "Pool delete #", s);
      }
      m_vec.clear();
      m_size = m_vec.size();
    }
    m_mtx.unlock();
  }
}

} // namespace cb

#endif