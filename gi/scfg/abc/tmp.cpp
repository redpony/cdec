#include <iostream>
#include <set>
#include <vector>
using namespace std;

int x = 5;

class A{A(){x++;}};
//  {
//   int a_;

// };

class B: public A{

  int b_;
};

int main(){

  cout<<"Hello World";
  set<int> s;

  s.insert(1);
  s.insert(2);

  x++;
  cout<<"x="<<x<<endl;

  vector<int> t;
  t.push_back(2); t.push_back(1); t.push_back(2); t.push_back(3); t.push_back(2); t.push_back(4);
  for(vector<int>::iterator it = t.begin(); it != t.end(); it++){
    if (*it ==2) t.erase(it);
    cout <<*it<<endl;
  }
}
