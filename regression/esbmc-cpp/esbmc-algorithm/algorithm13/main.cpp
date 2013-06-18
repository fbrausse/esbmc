// copy algorithm example
#include <iostream>
#include <algorithm>
#include <vector>
#include <cassert>
using namespace std;

int main () {
  int myints[]={10,20,30,40,50,60,70};
  vector<int> myvector;
  vector<int>::iterator it;

  myvector.resize(7);   // allocate space for 7 elements

  copy ( myints, myints+7, myvector.begin() );
  assert(myvector[0] == 10);
  assert(myvector[1] == 20);
  assert(myvector[2] == 30);
  assert(myvector[3] == 40);
  cout << "myvector contains:";
  for (it=myvector.begin(); it!=myvector.end(); ++it)
    cout << " " << *it;

  cout << endl;

  return 0;
}
