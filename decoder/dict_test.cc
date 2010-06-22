#include "dict.h"

#include "fdict.h"

#include <iostream>
#include <gtest/gtest.h>
#include <cassert>
#include "filelib.h"

#include "tdict.h"

using namespace std;

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

TEST_F(DTest, FDictTest) {
  int fid = FD::Convert("First");
  EXPECT_GT(fid, 0);
  EXPECT_EQ(FD::Convert(fid), "First");
  string x = FD::Escape("=");
  cerr << x << endl;
  EXPECT_NE(x, "=");
  x = FD::Escape(";");
  cerr << x << endl;
  EXPECT_NE(x, ";");
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

