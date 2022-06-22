#ifndef CB_COMMON_TIMES_H
#define CB_COMMON_TIMES_H

#include "include/cb/defines.h"

namespace cb {
namespace common {

namespace times {

unsigned long unixtime();
unsigned long long unixtimemilli();
struct tm* iso8601(char* timeexpr, unsigned long long utmilli);

} // namespace times

} // namespace common
} // namespace cb

#endif
