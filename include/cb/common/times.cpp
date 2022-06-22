#define _CRT_SECURE_NO_WARNINGS

#include <cstdio>
#include <cstring>
#include <ctime>

#if ((defined(_WIN32) || defined(_WIN64)) && !defined(__CYGWIN__) && !defined(__MINGW32__) && !defined(__MINGW64__))
// https://stackoverflow.com/questions/1676036/what-should-i-use-to-replace-gettimeofday-on-windows
# include <Windows.h>
# include <cstdint>
# if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
# define DELTA_EPOCH_IN_MICROSECS  116444736000000000Ui64 // CORRECT
# else
# define DELTA_EPOCH_IN_MICROSECS  116444736000000000ULL // CORRECT
# endif

// C2011: MSVC defines this in WinSock2.h!?
//struct timeval {
//  long tv_sec;
//  long tv_usec;
//};

//namespace {

// https://stackoverflow.com/questions/10905892/equivalent-of-gettimeday-for-windows
int gettimeofday(struct timeval *tp, struct timezone *tzp) {
  // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
  // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
  // until 00:00:00 January 1, 1970
  static const uint64_t EPOCH = ((uint64_t) DELTA_EPOCH_IN_MICROSECS);

  SYSTEMTIME system_time;
  FILETIME file_time;
  uint64_t time;

  GetSystemTime(&system_time);
  SystemTimeToFileTime(&system_time, &file_time);
  time = ((uint64_t) file_time.dwLowDateTime);
  time += ((uint64_t) file_time.dwHighDateTime) << 32;

  tp->tv_sec = (long) ((time - EPOCH) / 10000000L);
  tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
  return 0;
}

//}
#else
# include <sys/time.h>
#endif

#include "include/cb/common/times.h"

namespace cb {
namespace common {

namespace times {

unsigned long unixtime() {
  unsigned long rtn = 0;

  struct timeval tv;
  gettimeofday(&tv, NULL);
  rtn = tv.tv_sec;

  return rtn;
}

unsigned long long unixtimemilli() {
  unsigned long long rtn = 0;

  struct timeval tv;
  gettimeofday(&tv, NULL);
  rtn = static_cast<unsigned long long>(tv.tv_sec) * 1000000 + tv.tv_usec;

  return rtn;
}

struct tm* iso8601(char *timeexpr, unsigned long long utmilli) {
  struct tm *rtn;

  //char timeexpr[CB_DEFINES_H_LEN_ISO8601] = { '\0' };

  struct timeval tv;

  if (utmilli == 0) {
      gettimeofday(&tv, NULL);
  } else {
      tv.tv_sec = utmilli / 1000000;
      tv.tv_usec = utmilli - static_cast<unsigned long long>(tv.tv_sec) * 1000000;
  }

  time_t rawtime = tv.tv_sec; //time(NULL);
  rtn = gmtime(&rawtime); //rtn = localtime(&rawtime);

  strftime(timeexpr, CB_DEFINES_H_LEN_ISO8601, "%Y-%m-%dT%H:%M:%S", rtn);
  sprintf(&timeexpr[strlen(timeexpr)], ".%03ldZ", tv.tv_usec / 1000);

  return rtn;
}

} // namespace times

} // namespace common
} // namespace cb
