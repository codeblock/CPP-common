#ifndef CB_COMMON_DEFINES_H_
#define CB_COMMON_DEFINES_H_

// YYYY-MM-DDTHH:MM:SS.sssZ
#define CB_DEFINES_H_LEN_ISO8601 25

# if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#   include <Windows.h>
#   define PATH_SEP '\\'
#   define CLEAR_CONSOLE system("cls")
#   define WAIT_A_SECONDS(s) Sleep((s) * 1000)
#   define WAIT_A_MILLISECONDS(ms) Sleep(ms)
# else
#   include <unistd.h>
#   define PATH_SEP '/'
#   define CLEAR_CONSOLE system("clear")
#   define WAIT_A_SECONDS(s) sleep(s)
#   define WAIT_A_MILLISECONDS(ms) usleep((ms) * 1000)
# endif

#endif