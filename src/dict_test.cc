#include "dict.h"

#include <gtest/gtest.h>
#include <cassert>

class DTest : public testing::Test {
 public:
  DTest() {}
 protected:
  virtual void SetUp() { }
  virtual void TearDown() { }
};

TEST_F(DTest, Convert) {
  Dict d;
  WordID a = d.Convert("foo");
  WordID b = d.Convert("bar");
  std::string x = "foo";
  WordID c = d.Convert(x);
  EXPECT_NE(a, b);
  EXPECT_EQ(a, c);
  EXPECT_EQ(d.Convert(a), "foo");
  EXPECT_EQ(d.Convert(b), "bar");
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

