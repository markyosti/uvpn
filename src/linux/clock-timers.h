// Copyright (c) 2008,2009,2010,2011 Mark Moreno (kramonerom@gmail.com).
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
//    1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 
//    2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY Mark Moreno ''AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL Mark Moreno OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// The views and conclusions contained in the software and documentation are
// those of the authors and should not be interpreted as representing official
// policies, either expressed or implied, of Mark Moreno.

#ifndef CLOCK_TIMERS_H
# define CLOCK_TIMERS_H

# include "../base.h"
# include "../errors.h"
# include <time.h>

class ClockTimer {
 public:
  // Time in ms, integer. 32 bits allow delays up to 49 days.
  typedef unsigned int ms_timer_t;

  static ms_timer_t kTimerNone;
  static ms_timer_t kTimerMax;

  explicit ClockTimer(ms_timer_t interval) : ts_(GetCurrentTime()) {
    IncrementBy(interval);
  }

  ClockTimer(const ClockTimer& other, ms_timer_t interval) {
    ts_.tv_sec = other.ts_.tv_sec;
    ts_.tv_nsec = other.ts_.tv_nsec;

    IncrementBy(interval);
  }

  ClockTimer() : ts_(GetCurrentTime()) {
  }

  void IncrementBy(ms_timer_t amount) {
    ts_.tv_sec +=
      ((ts_.tv_nsec + ((amount % 1000) * 1000000)) / 1000000000) + (amount / 1000);
    ts_.tv_nsec = (ts_.tv_nsec + ((amount % 1000) * 1000000)) % 1000000000;
  }

  bool IsBefore(const ClockTimer& other) const {
    LOG_DEBUG("this tv_sec, tv_nsec: %ld, %ld, other: %ld %ld",
              ts_.tv_sec, ts_.tv_nsec, other.ts_.tv_sec, other.ts_.tv_nsec);
    if (ts_.tv_sec < other.ts_.tv_sec)
      return true;

    if (ts_.tv_sec == other.ts_.tv_sec)
      if (ts_.tv_nsec <= other.ts_.tv_nsec)
        return true;

    return false;
  }

  ms_timer_t GetDelayFrom(const ClockTimer& other) const {
    if (IsBefore(other))
      return 0;

    time_t delta_sec = (ts_.tv_sec - other.ts_.tv_sec);
    if (delta_sec < 0 ||
        delta_sec >= static_cast<time_t>(kTimerMax / 1000))
      return kTimerMax;

    ms_timer_t result = delta_sec * 1000 +
        (ts_.tv_nsec - other.ts_.tv_nsec) / 1000000;
    if (static_cast<time_t>(result / 1000) < delta_sec || result > kTimerMax)
      return kTimerMax;

    return result;
  }

 private:
  static timespec GetCurrentTime() {
    timespec ts;
    // TODO: check for errors?
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts;
  }

  timespec ts_;
};

#endif /* CLOCK_TIMERS_H */