#ifndef CB_COMMON_TIMES_H_
#define CB_COMMON_TIMES_H_

#include "include/cb/common/defines.h"

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
