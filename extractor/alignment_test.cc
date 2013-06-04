#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include "alignment.h"

using namespace std;
using namespace ::testing;
namespace ar = boost::archive;

namespace extractor {
namespace {

class AlignmentTest : public Test {
 protected:
  virtual void SetUp() {
    alignment = Alignment("sample_alignment.txt");
  }

  Alignment alignment;
};

TEST_F(AlignmentTest, TestGetLinks) {
  vector<pair<int, int>> expected_links = {
    make_pair(0, 0), make_pair(1, 1), make_pair(2, 2)
  };
  EXPECT_EQ(expected_links, alignment.GetLinks(0));
  expected_links = {make_pair(1, 0), make_pair(2, 1)};
  EXPECT_EQ(expected_links, alignment.GetLinks(1));
}

TEST_F(AlignmentTest, TestSerialization) {
  stringstream stream(ios_base::binary | ios_base::out | ios_base::in);
  ar::binary_oarchive output_stream(stream, ar::no_header);
  output_stream << alignment;

  Alignment alignment_copy;
  ar::binary_iarchive input_stream(stream, ar::no_header);
  input_stream >> alignment_copy;

  EXPECT_EQ(alignment, alignment_copy);
}

} // namespace
} // namespace extractor
