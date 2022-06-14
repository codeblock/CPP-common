#define _CRT_SECURE_NO_WARNINGS

#ifndef CB_LOGGER_H_
#define CB_LOGGER_H_

#include <cstdio>
#include <mutex>

#include "include/cb/defines.h"

#define CB_LOGGER_H_LEN_PATH 256
#define CB_LOGGER_H_LEN_CONTENTS 512

namespace cb {

class Logger {
public:
  enum class eLevel : unsigned short {
    eEror = 0,
    eWarn,
    eInfo,
    eDebug,
  };
  static const char* const LEVELS[];

private:
  Logger(void);
  Logger(const Logger& rhw);
  Logger& operator=(const Logger& rhw);
  ~Logger(void);

private:
  std::mutex m_mtx;
  unsigned int m_cnt;
  unsigned int m_split;
  char m_path[CB_LOGGER_H_LEN_PATH];
  char m_currinfo[CB_DEFINES_H_LEN_ISO8601];
  FILE* m_fp;

public:
  static unsigned int setSplit(unsigned int split);
  static bool setPath(const char* path);
  static void log(eLevel level, const char* format, ...);

private:
  void writeFile(eLevel level, const char* timeinfo, const char* currinfo, const char* filename, const char* filecontents);
private:
  static Logger m_instance;
};

} // namespace cb

#endif