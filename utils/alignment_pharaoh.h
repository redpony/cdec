#ifndef _PHARAOH_ALIGNMENT_H_
#define _PHARAOH_ALIGNMENT_H_

#include <string>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include "array2d.h"

struct AlignmentPharaoh {
  static boost::shared_ptr<Array2D<bool> > ReadPharaohAlignmentGrid(const std::string& al);
  static void SerializePharaohFormat(const Array2D<bool>& alignment, std::ostream* out);
};

#endif
