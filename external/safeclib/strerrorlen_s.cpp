#include "strerrorlen_s.h"

#include <cstring>

namespace safeclib
{

size_t strerrorlen_s(int errnum)
{
#ifndef ESNULLP
#define ESNULLP (400) /* null ptr */
#endif

#ifndef ESLEWRNG
#define ESLEWRNG (410) /* wrong size */
#endif

#ifndef ESLAST
#define ESLAST ESLEWRNG
#endif

  static const int len_errmsgs_s[] = {
    sizeof "null ptr",                 /* ESNULLP */
    sizeof "length is zero",           /* ESZEROL */
    sizeof "length is below min",      /* ESLEMIN */
    sizeof "length exceeds RSIZE_MAX", /* ESLEMAX */
    sizeof "overlap undefined",        /* ESOVRLP */
    sizeof "empty string",             /* ESEMPTY */
    sizeof "not enough space",         /* ESNOSPC */
    sizeof "unterminated string",      /* ESUNTERM */
    sizeof "no difference",            /* ESNODIFF */
    sizeof "not found",                /* ESNOTFND */
    sizeof "wrong size",               /* ESLEWRNG */
  };

  if (errnum >= ESNULLP && errnum <= ESLAST) {
    return len_errmsgs_s[errnum - ESNULLP] - 1;
  }
  else {
#if defined(_MSC_VER)
    char buf[256]{};
    return (0 == strerror_s(buf, sizeof(buf), errnum)) ? strlen(buf) : 0;
#else
    const char* buf = strerror(errnum);
    return buf ? strlen(buf) : 0;
#endif
  }
}

} // namespace safeclib
