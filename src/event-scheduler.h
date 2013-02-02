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

#ifndef EVENT_SCHEDULER_H
# define EVENT_SCHEDULER_H

# include "base.h"
# include "timers.h"
# include "errors.h"

# include <queue>

class EventScheduler {
 public:
  class Event {
   public:
    typedef function<bool ()> timer_handler_t;

    Event(const char* name, timer_handler_t* callback)
        : name_(name), callback_(callback) {}

    const Timer& GetWhen() const { return timer_; }
    const char* GetName() const { return name_; }

    void SetCallback(timer_handler_t* callback) { callback_ = callback; }

    virtual void Run(EventScheduler* scheduler, const Timer& now) = 0;

   protected:
    void SetWhen(const Timer& timer) { timer_ = timer; }
    timer_handler_t* GetCallback() { return callback_; }

   private:
    const char* name_;
    Timer timer_;

    timer_handler_t* callback_;
  };

  void AddEvent(Event* event) { queue_.push(event); }

  Timer::ms_timer_t GetTimeUntilNextEvent() const { 
    return GetTimeUntilNextEvent(Timer());
  }

  Timer::ms_timer_t GetTimeUntilNextEvent(const Timer& now) const { 
    const Event* event(PeekAtNextEvent());
    if (!event)
      return Timer::kTimerMax;

    return event->GetWhen().GetDelayFrom(now);
  }

  void ProcessPendingEvents() {
    return ProcessPendingEvents(Timer());
  }

  void ProcessPendingEvents(const Timer& now) {
    while (!queue_.empty()) {
      Event* event(queue_.top());
      if (!event->GetWhen().IsBefore(now) && !event->GetWhen().IsSame(now))
        return;

      LOG_DEBUG("running event '%s'", event->GetName());
      event->Run(this, now);
      queue_.pop();
    }
  }

 private:
  const Event* PeekAtNextEvent() const {
    return queue_.empty() ? NULL : queue_.top();
  }

  struct EventOrdering {
    bool operator()(const EventScheduler::Event* first,
                    const EventScheduler::Event* second) const {
      return (*this)(*first, *second);
    }
  
    bool operator()(const EventScheduler::Event& first,
                    const EventScheduler::Event& second) const {
      return second.GetWhen().IsBefore(first.GetWhen());
    }
  };

  priority_queue<Event*, vector<Event*>, EventOrdering> queue_;
};

class OneOffEvent : public EventScheduler::Event {
 public:
  OneOffEvent(const char* name, timer_handler_t* callback)
      : EventScheduler::Event(name, callback) {}

  void Start(EventScheduler* scheduler, Timer::ms_timer_t delay) {
    Start(scheduler, Timer(delay));
  }

  void Start(EventScheduler* scheduler, const Timer& start) {
    SetWhen(start);
    scheduler->AddEvent(this);
  }

  virtual void Run(EventScheduler* scheduler, const Timer& now) {
    (*GetCallback())();
  }
};

class ExponentialBackoffEvent : public EventScheduler::Event {
 public:
  ExponentialBackoffEvent(
      const char* name, timer_handler_t* callback)
      : EventScheduler::Event(name, callback) {}

  void Start(EventScheduler* scheduler, const Timer& start,
             Timer::ms_timer_t initial, Timer::ms_timer_t max) {
    start_ = start;
    delay_ = initial;
    max_ = max;

    SetWhen(Timer(start_, delay_));
    scheduler->AddEvent(this);
  }

  virtual void Run(EventScheduler* scheduler, const Timer& now) {
    if ((*GetCallback())())
      return;

    Timer::ms_timer_t next(delay_ << 1);
    // Note that delay_ is always used relative to the first event.
    if ((delay_ + next) < delay_ || next >= max_)
      delay_ = delay_ + max_;
    else
      delay_ = delay_ + next;

    LOG_DEBUG("scheduling in %d since first event", delay_);
    SetWhen(Timer(start_, delay_));
    scheduler->AddEvent(this);
  }

 private:
  Timer start_;
  Timer::ms_timer_t delay_;
  Timer::ms_timer_t max_;
};

#endif /* EVENT_SCHEDULER_H */
