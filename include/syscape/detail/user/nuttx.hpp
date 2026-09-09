#ifndef SYSCAPE_DETAIL_USER_NUTTX_HPP
#define SYSCAPE_DETAIL_USER_NUTTX_HPP

// Native NuttX without user identities implements the POSIX credential calls
// as synthetic root values. Do not expose those placeholders as observations.
#if !defined(__NuttX__) || defined(CONFIG_SCHED_USER_IDENTITY)
#include <syscape/detail/user/posix.hpp>
#else
#include <syscape/detail/user/generic.hpp>
#endif

#endif
