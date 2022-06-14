#ifndef CB_TIMES_H
#define CB_TIMES_H

#include "include/cb/defines.h"

namespace cb {

namespace times {

unsigned long unixtime();
unsigned long long unixtimemilli();
struct tm* iso8601(char* timeexpr, unsigned long long utmilli);

} // namespace times

} // namespace cb

#endif