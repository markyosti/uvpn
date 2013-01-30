#include "gtest.h"
#include "src/buffer.h"

TEST(BufferTest, Base) {
  Buffer buffer;

  EXPECT_EQ(0u, buffer.Input()->ContiguousSize());

  const char data[] = "this is a random string (not really)";
  buffer.Input()->Add(data, sizeof(data));

  // 1 << 14 is kRoundedSize, not accessible from here.
  // TODO: make kRoundedSize accessible.
  EXPECT_EQ((1 << 14)  - sizeof(data), (unsigned int)buffer.Input()->ContiguousSize());

  EXPECT_EQ(sizeof(data), (unsigned int)buffer.Output()->ContiguousSize());
  EXPECT_EQ(sizeof(data), (unsigned int)buffer.Output()->LeftSize());
  EXPECT_EQ(0, buffer.Output()->Offset());

  LOG_DEBUG("%s", buffer.Output()->Data());
  EXPECT_EQ(0, memcmp(data, buffer.Output()->Data(), buffer.Output()->ContiguousSize()));
}

TEST(BufferTest, AddWrapsCorrectly) {
  Buffer buffer;
  const char data[] = "this is a random string (not really)";
  int size = sizeof(data);

  // Add lot of crap first.
  for (int  i = 0; i < 8192; i++)
    buffer.Input()->Add(data, size);

  EXPECT_EQ(16354u, buffer.Output()->ContiguousSize());
  EXPECT_EQ(8192 * size, buffer.Output()->LeftSize());

  LOG_DEBUG("size %d (%d)", 8192 * size, size);

  OutputCursor output(*buffer.Output());
  EXPECT_EQ(output.Data(), buffer.Output()->Data());
  EXPECT_EQ(output.LeftSize(), buffer.Output()->LeftSize());
  EXPECT_EQ(output.ContiguousSize(), buffer.Output()->ContiguousSize());

  // Ok, read some data out of the buffer.
  int ssize = (4096 * size) - 10;
  char* space = new char[ssize];

  EXPECT_EQ(ssize - 1, output.Consume(space, ssize - 1));
  space[ssize - 1] = '\0';

  EXPECT_EQ(8192 * size - (ssize - 1), output.LeftSize());
  EXPECT_EQ(11999, output.ContiguousSize());

  int lsize = (4096 * size) + 100;
  char* left = new char[lsize];
  EXPECT_EQ(4096 * size + 10 + 1, output.Consume(left, lsize - 1));

  delete [] space;
  delete [] left;
}

TEST(BufferTest, ConsumeString) {
  Buffer buffer;
  const char data[] = "this is a random string (not really)";
  int size = sizeof(data);

  string comparison;
  comparison.reserve(size * 8192);
  for (int i = 0; i < 8192; i++) {
    comparison.append(data, size - 1);
    buffer.Input()->Add(data, size - 1);
  }
  buffer.Input()->Add("", 1);
  comparison.append("", 1);

  OutputCursor outcursor(*buffer.Output());
  ASSERT_EQ(comparison.size(), static_cast<unsigned int>(outcursor.LeftSize()));
  string filled;
  outcursor.ConsumeString(&filled);
  EXPECT_EQ(comparison, filled);
}

TEST(BufferTest, DirectAccess) {
  Buffer buffer;
  const char data[] = "this is a random string (not really)";
  int size = sizeof(data);

  buffer.Input()->Reserve(10 * size);
  EXPECT_LE(10 * size, buffer.Input()->ContiguousSize());

  int leftsize = buffer.Input()->ContiguousSize();
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(leftsize, buffer.Input()->ContiguousSize());
    memcpy(buffer.Input()->Data(), data, size);
    buffer.Input()->Increment(size);
    leftsize -= size;
  }

  OutputCursor indata(*buffer.Output());
  EXPECT_EQ(10 * size, indata.ContiguousSize());

  char tmp[size];
  leftsize = 10 * size;
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(leftsize, indata.LeftSize());

    memcpy(tmp, indata.Data(), size);
    EXPECT_EQ(string(data), string(tmp)) << "iteration " << i;
    indata.Increment(size);
    leftsize -= size;
  }
}

TEST(BufferTest, Get) {
  Buffer buffer;
  const char data[] = "this is a random string (not really)";
  int size = sizeof(data);

  string comparison;
  comparison.reserve(size * 8192);
  for (int i = 0; i < 8192; i++) {
    comparison.append(data, size - 1);
    buffer.Input()->Add(data, size - 1);
  }
  buffer.Input()->Add("", 1);
  comparison.append("", 1);

  // Allocate a buffer larger than needed, as we will try different things.
  char temp[2 * (sizeof(data) * 8192 + 1)];

  int expected_left_size = buffer.Output()->LeftSize();
  int expected_contiguous_size = buffer.Output()->ContiguousSize();
  ASSERT_EQ((sizeof(data) - 1) * 8192 + 1, expected_left_size);

  // Short Get, nothing should be updated, data should be correct.
  EXPECT_EQ(10, buffer.Output()->Get(temp, 10));
  EXPECT_EQ(expected_left_size, buffer.Output()->LeftSize());
  EXPECT_EQ(expected_contiguous_size, buffer.Output()->ContiguousSize());
  EXPECT_EQ(0, memcmp(comparison.c_str(), temp, 10));

  // Long Get, nothing should be updated, only some data copied. 
  EXPECT_EQ(expected_left_size,
            buffer.Output()->Get(temp, 3 * expected_left_size));
  EXPECT_EQ(expected_left_size, buffer.Output()->LeftSize());
  EXPECT_EQ(expected_contiguous_size, buffer.Output()->ContiguousSize());
  EXPECT_EQ(0, memcmp(comparison.c_str(), temp, expected_contiguous_size));

  // Exact Get, same as above.
  EXPECT_EQ(expected_left_size,
            buffer.Output()->Get(temp, expected_left_size));
  EXPECT_EQ(expected_left_size, buffer.Output()->LeftSize());
  EXPECT_EQ(expected_contiguous_size, buffer.Output()->ContiguousSize());
  EXPECT_EQ(0, memcmp(comparison.c_str(), temp, expected_left_size));

  // 0 Get.
  EXPECT_EQ(0, buffer.Output()->Get(temp, 0));
  EXPECT_EQ(expected_left_size, buffer.Output()->LeftSize());
  EXPECT_EQ(expected_contiguous_size, buffer.Output()->ContiguousSize());

  // Get on empty buffer.
  Buffer empty;
  EXPECT_EQ(0, empty.Output()->Get(temp, 0));
  EXPECT_EQ(0, empty.Output()->Get(temp, 100));
}

TEST(BufferTest, GetIovec) {
  Buffer buffer;
  const char data[] = "this is a random string (not really)";
  int dsize = sizeof(data);

  // Add lot of crap first.
  for (int  i = 0; i < 8398; i++)
    buffer.Input()->Add(data, dsize);

  OutputCursor cursor(*buffer.Output());
  OutputCursor::Iovec iov;

  unsigned int iovsize; 
  unsigned int size = cursor.GetIovec(&iov, &iovsize);
  EXPECT_EQ(16, iovsize);
  EXPECT_EQ(261664, size);
  cursor.Increment(size);

  size = cursor.GetIovec(&iov, &iovsize);
  EXPECT_EQ(3, iovsize);
  EXPECT_EQ(49062, size);
  cursor.Increment(size);

  size = cursor.GetIovec(&iov, &iovsize);
  EXPECT_EQ(0, iovsize);
  EXPECT_EQ(0, size);
}

TEST(BufferTest, MultipleOutputCursors) {
  Buffer buffer;
  const char data[] = "this is a random string (not really)";
  int size = sizeof(data);

  // Add lot of crap first.
  for (int  i = 0; i < 7072; i++)
    buffer.Input()->Add(data, size);

  OutputCursor cursor1(*buffer.Output());
  OutputCursor cursor2(*buffer.Output());

  char space1[4000];
  char space2[4000];

  EXPECT_EQ(261664, cursor1.LeftSize());
  EXPECT_EQ(261664, cursor2.LeftSize());
  EXPECT_EQ(261664, buffer.Output()->LeftSize());

  EXPECT_EQ(sizeof(space1), cursor1.Consume(space1, sizeof(space1)));
  EXPECT_EQ(257664, cursor1.LeftSize());
  EXPECT_EQ(261664, cursor2.LeftSize());
  EXPECT_EQ(261664, buffer.Output()->LeftSize());

  EXPECT_EQ(sizeof(space2), cursor2.Consume(space2, sizeof(space2)));
  EXPECT_EQ(257664, cursor1.LeftSize());
  EXPECT_EQ(257664, cursor2.LeftSize());
  EXPECT_EQ(261664, buffer.Output()->LeftSize());
}

TEST(BufferTest, ProduceConsumeProduceConsume) {
  Buffer buffer;
  const char data[] = "this is a random string (not really)";
  int size = sizeof(data) - 1;

  // Inspired by Star-Wars, the emperor strikes back.
  const char data2[] = "Ta ra ra ra-ra-ra ra-ra-ra. Ta ra ra ra-ra-ra ra-ra-ra";
  int size2 = sizeof(data2) - 1;

  buffer.Input()->Add(data, size);
  EXPECT_EQ(size, buffer.Output()->LeftSize());

  string read;
  buffer.Output()->ConsumeString(&read);
  EXPECT_EQ(data, read);
  EXPECT_EQ(0, buffer.Output()->LeftSize());

  buffer.Input()->Add(data2, size2);
  EXPECT_EQ(size2, buffer.Output()->LeftSize());

  read.clear();
  buffer.Output()->ConsumeString(&read);
  EXPECT_EQ(data2, read);
  EXPECT_EQ(0, buffer.Output()->LeftSize());
}
