#include "gtest.h"
#include "src/event-scheduler.h"

class SchedulerTest : public testing::Test {
 protected:
  SchedulerTest()
      : event_foo_(bind(&SchedulerTest::Callback, this, "foo")),
        event_bar_(bind(&SchedulerTest::Callback, this, "bar")),
        event_baz_(bind(&SchedulerTest::Callback, this, "baz")) {}

  bool Callback(const char* str) {
    events_.push_back(str);

    bool retval = false;
    if (!retvals_.empty()) {
      retval = retvals_[retvals_.size() - 1];
      retvals_.pop_back();
    }
    return retval;
  }

  EventScheduler::Event::timer_handler_t event_foo_;
  EventScheduler::Event::timer_handler_t event_bar_;
  EventScheduler::Event::timer_handler_t event_baz_;

  vector<string> events_;
  vector<bool> retvals_;

  EventScheduler scheduler_;
};

TEST_F(SchedulerTest, Basic) {
  // No event scheduled, wait as long as we can.
  EXPECT_EQ(Timer::kTimerMax, scheduler_.GetTimeUntilNextEvent());
  // There are no events, nothing should happen here.
  scheduler_.ProcessPendingEvents();
}

TEST_F(SchedulerTest, SomeOneOffEvents) {
  OneOffEvent ev_foo("foo", NULL);
  OneOffEvent ev_bar("bar", &event_bar_);
  OneOffEvent ev_baz("baz", &event_baz_);

  ev_foo.SetCallback(&event_foo_);

  Timer now;
  Timer future(now);

  ev_foo.Start(&scheduler_, Timer(now, 10));
  ev_bar.Start(&scheduler_, Timer(now, 2));
  ev_baz.Start(&scheduler_, Timer(now, 7));

  future.IncrementBy(5);

  EXPECT_EQ(static_cast<Timer::ms_timer_t>(2), scheduler_.GetTimeUntilNextEvent(now));
  EXPECT_EQ(static_cast<Timer::ms_timer_t>(0), scheduler_.GetTimeUntilNextEvent(future));
  EXPECT_TRUE(events_.empty());

  future.IncrementBy(3);

  scheduler_.ProcessPendingEvents(future);

  ASSERT_EQ(static_cast<size_t>(2), events_.size());
  EXPECT_EQ(events_[0], "bar");
  EXPECT_EQ(events_[1], "baz");

  future.IncrementBy(3);
  scheduler_.ProcessPendingEvents(future);
  ASSERT_EQ(static_cast<size_t>(3), events_.size());
  EXPECT_EQ(events_[2], "foo");
}

TEST_F(SchedulerTest, SomeExponentialBackoffs) {
  ExponentialBackoffEvent ev_foo("foo", &event_foo_);
  OneOffEvent ev_bar("bar", &event_bar_);

  Timer now;
  Timer future(now);

  ev_foo.Start(&scheduler_, now, 3, 36);
  ev_bar.Start(&scheduler_, Timer(now, 5));

  // After 2 ms, nothing should happen.
  future.IncrementBy(2);
  scheduler_.ProcessPendingEvents(future);
  EXPECT_TRUE(events_.empty());

  // We are at 3, the exponential back off should be run and fail.
  future.IncrementBy(1);
  scheduler_.ProcessPendingEvents(future);
  ASSERT_EQ(static_cast<size_t>(1), events_.size());
  EXPECT_EQ("foo", events_[0]);

  // We are at 5 now. The one off should be run.
  future.IncrementBy(2);
  scheduler_.ProcessPendingEvents(future);
  ASSERT_EQ(static_cast<size_t>(2), events_.size());
  EXPECT_EQ("bar", events_[1]);

  // 9, second attempt at the backoff event. (3 + 3 * 2)
  future.IncrementBy(4);
  scheduler_.ProcessPendingEvents(future);
  ASSERT_EQ(static_cast<size_t>(3), events_.size());
  EXPECT_EQ("foo", events_[2]);

  // Next attempt will be at  9 + 9 * 2 = 27, so +18, let's see that
  // it doesn't trigger too early.
  future.IncrementBy(17);
  scheduler_.ProcessPendingEvents(future);
  ASSERT_EQ(static_cast<size_t>(3), events_.size());
  // And that it computes the dealy correctly.
  EXPECT_EQ(static_cast<Timer::ms_timer_t>(1), scheduler_.GetTimeUntilNextEvent(future));

  // Now it should trigger.
  future.IncrementBy(1);
  scheduler_.ProcessPendingEvents(future);
  ASSERT_EQ(static_cast<size_t>(4), events_.size());
  EXPECT_EQ("foo", events_[3]);

  // We are at 27 now, next interval should be 27 + 27 * 2 >= 36, which
  // is the max we set. So... we should wait at most 36 ms for the next
  // event.
  EXPECT_EQ(static_cast<Timer::ms_timer_t>(36),
            scheduler_.GetTimeUntilNextEvent(future));

  // Let's say that now we finally return success.
  future.IncrementBy(36);
  retvals_.push_back(true);
  scheduler_.ProcessPendingEvents(future);
  ASSERT_EQ(static_cast<size_t>(5), events_.size());
  EXPECT_EQ("foo", events_[4]);

  // We should end up with no other events scheduled.
  EXPECT_EQ(Timer::kTimerMax, scheduler_.GetTimeUntilNextEvent());
}

