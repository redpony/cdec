#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <gtest/gtest.h>
#include "weights.h"
#include "tdict.h"
#include "hg.h"

using namespace std;

class WeightsTest : public testing::Test {
 protected:
  virtual void SetUp() { }
  virtual void TearDown() { }
};
       

TEST_F(WeightsTest,Load) {
  Weights w;
  w.InitFromFile("test_data/weights");
  w.WriteToFile("-");
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
