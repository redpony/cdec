#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "alignment.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

class AlignmentTest : public Test {
 protected:
  virtual void SetUp() {
    alignment = make_shared<Alignment>("sample_alignment.txt");
  }

  shared_ptr<Alignment> alignment;
};

TEST_F(AlignmentTest, TestGetLinks) {
  vector<pair<int, int> > expected_links = {
    make_pair(0, 0), make_pair(1, 1), make_pair(2, 2)
  };
  EXPECT_EQ(expected_links, alignment->GetLinks(0));
  expected_links = {make_pair(1, 0), make_pair(2, 1)};
  EXPECT_EQ(expected_links, alignment->GetLinks(1));
}

} // namespace
} // namespace extractor
