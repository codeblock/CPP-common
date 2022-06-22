#define _CRT_SECURE_NO_WARNINGS

#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cstdarg>

#include "include/cb/defines.h"
#include "include/cb/common/times.h"
#include "include/cb/common/logger.h"

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
# include <direct.h>
#endif

namespace cb {
namespace common {

Logger::Logger(void) {
  m_cnt = 0;
  m_split = 10;
  m_fp = NULL;
  m_currinfo[CB_DEFINES_H_LEN_ISO8601] = { '\0' };
  m_path[CB_COMMON_LOGGER_H_LEN_PATH] = { '\0' };
  // fopen
}

Logger::~Logger(void) {
  if (m_fp != NULL) {
    m_instance.m_mtx.lock();
    if (m_fp != NULL) {
      fclose(m_fp);
    }
    m_instance.m_mtx.unlock();
  }
  // printf("m_cnt: %u\n", m_cnt);
}

void Logger::writeFile(eLevel level, const char* timeinfo, const char* currinfo, const char* filename, const char* filecontents) {
  if (strlen(m_path) == 0) {
    return;
  }

  if (m_fp == NULL) {
    m_instance.m_mtx.lock();
    if (m_fp == NULL) {
      m_fp = fopen(filename, "a");
      strcpy(m_instance.m_currinfo, currinfo);
      //fprintf(m_fp, "[%s][%s] file open\n", timeinfo, LEVELS[static_cast<int>(level)]); // @debug
    }
    m_instance.m_mtx.unlock();
  } else if (strcmp(m_instance.m_currinfo, currinfo) != 0) {
    m_instance.m_mtx.lock();
    if (strcmp(m_instance.m_currinfo, currinfo) != 0) {
      fclose(m_fp);
      m_fp = fopen(filename, "a");
      strcpy(m_instance.m_currinfo, currinfo);
      //fprintf(m_fp, "[%s][%s] file close, open\n", timeinfo, LEVELS[static_cast<int>(level)]); // @debug
    }
    m_instance.m_mtx.unlock();
  }

  if (m_fp != NULL) {
    fprintf(m_fp, "[%s][%s] %s\n", timeinfo, LEVELS[static_cast<int>(level)], filecontents);
    printf("[%s][%s] %s\n", timeinfo, LEVELS[static_cast<int>(level)], filecontents);
  }

  // closed in destructor
}

unsigned int Logger::setSplit(unsigned int split) {
  m_instance.m_split = split;

  return m_instance.m_split;
}

bool Logger::setPath(const char* path)
{
  bool rtn = true;
  char path_tmp[CB_COMMON_LOGGER_H_LEN_PATH] = {};
  struct stat sb;

  strcpy(path_tmp, path);
  if (path_tmp[strlen(path_tmp) - 1] == PATH_SEP) {
    path_tmp[strlen(path_tmp) - 1] = '\0';
  }
  //if (strlen(LOGDIR) == 0) {
  memcpy((void*)m_instance.m_path, path_tmp, CB_COMMON_LOGGER_H_LEN_PATH);
  //}

  rtn = stat(path_tmp, &sb);
  if (rtn == false) {
    // 0. already exists
    return true;
  }

  rtn = ((sb.st_mode & S_IFDIR) == 1);
  if (rtn == false) {
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
# pragma warning(suppress : 4996)
    if (mkdir(path_tmp) == 0) {
#else
    if (mkdir(path_tmp, 0755) == 0) {
#endif
      rtn = true;
    }
  }

  return rtn;
}

void Logger::log(eLevel level, const char* format, ...)
{
  va_list argList;

  struct tm* ptm;
  char timeinfo[CB_DEFINES_H_LEN_ISO8601] = { '\0' };
  char currinfo[CB_DEFINES_H_LEN_ISO8601] = { '\0' };
  char filename[CB_COMMON_LOGGER_H_LEN_PATH] = { '\0' };
  char filecontents[CB_COMMON_LOGGER_H_LEN_CONTENTS] = { '\0' };

  ptm = cb::common::times::iso8601(timeinfo, 0);

  strftime(currinfo, CB_DEFINES_H_LEN_ISO8601, "%Y-%m-%d-%H", ptm);
  sprintf(&currinfo[strlen(currinfo)], "-%02d", ptm->tm_min / m_instance.m_split % m_instance.m_split);
  //printf("-------------- %d / %s\n", m_instance.m_split, currinfo);

  strcpy(filename, m_instance.m_path);
  filename[strlen(filename)] = PATH_SEP;
  strcat(filename, "server.");
  strcat(filename, currinfo);
  strcat(filename, ".log");

  va_start(argList, format);
  vsprintf(filecontents, format, argList);
  if (strlen(filecontents) == 0)
  {
    sprintf(filecontents, "%s", format);
  }
  va_end(argList);

  m_instance.writeFile(level, timeinfo, currinfo, filename, filecontents);

  //m_instance.m_mtx.lock();
  //m_instance.m_cnt++;
  //m_instance.m_mtx.unlock();
}

const char* const Logger::LEVELS[] = {
  "ERROR",
  "WARN ",
  "INFO ",
  "DEBUG",
};
Logger Logger::m_instance = Logger();

} // namespace common
} // namespace cb
