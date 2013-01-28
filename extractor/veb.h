#ifndef _VEB_H_
#define _VEB_H_

#include <memory>

using namespace std;

class VEB {
 public:
  static shared_ptr<VEB> Create(int size);

  virtual void Insert(int value) = 0;

  virtual int GetSuccessor(int value) = 0;

  int GetMinimum();

  int GetMaximum();

  static int MIN_BOTTOM_BITS;
  static int MIN_BOTTOM_SIZE;

 protected:
  VEB(int min = -1, int max = -1);

  int min, max;
};

#endif
