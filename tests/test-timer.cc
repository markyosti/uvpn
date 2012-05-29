#include "gtest.h"
#include "src/timers.h"

TEST(Timer, IsBefore) {
  Timer mostfuture(10000);
  Timer somefuture(5000);
  Timer now;

  EXPECT_TRUE(now.IsBefore(mostfuture));
  EXPECT_TRUE(now.IsBefore(somefuture));
  EXPECT_TRUE(somefuture.IsBefore(mostfuture));

  EXPECT_FALSE(somefuture.IsBefore(now));
  EXPECT_FALSE(mostfuture.IsBefore(somefuture));
  EXPECT_FALSE(somefuture.IsBefore(somefuture));
}

TEST(Timer, GetDelayFrom) {
  Timer now;
  Timer future(now);
  future.IncrementBy(123456);

  EXPECT_EQ(static_cast<Timer::ms_timer_t>(123456), future.GetDelayFrom(now));
}
