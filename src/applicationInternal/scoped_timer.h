#ifndef SCOPED_TIMER_H
#define SCOPED_TIMER_H

#include "omote_log.h"

#ifdef ENABLE_BOOT_PROFILING

class ScopedTimer {
    const char* name;
    unsigned long start;
public:
    ScopedTimer(const char* n) : name(n), start(millis()) {}
    ~ScopedTimer() {
        omote_log_w("%s took %lu ms", name, millis() - start);
    }
};

#define SCOPED_TIMER() ScopedTimer _scopedTimer(__func__)
#define SCOPED_TIMER_NAMED(name) ScopedTimer _scopedTimer_##name(#name)

#else

#define SCOPED_TIMER() do {} while(0)
#define SCOPED_TIMER_NAMED(name) do {} while(0)

#endif // ENABLE_BOOT_PROFILING

#endif /* SCOPED_TIMER_H */
