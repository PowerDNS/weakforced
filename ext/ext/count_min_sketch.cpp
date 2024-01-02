/** 
    Author: Daniel Alabi
    http://alabidan.me
    GitHub: alabid/countminsketch
    Count-Min Sketch Implementation based on paper by
    Muthukrishnan and Cormode, 2004
**/

# include <iostream>
# include <cmath>
# include <cstdlib>
# include <ctime>
# include <limits>
# include <sstream>
# include <algorithm>
# include "count_min_sketch.hpp"
using namespace std;

/**
   Class definition for CountMinSketch.
   public operations:
   // overloaded updates
   void update(int item, int c);
   void update(char *item, int c);
   // overloaded estimates
   unsigned int estimate(int item);
   unsigned int estimate(char *item);
**/


// CountMinSketch constructor
// ep -> error 0.01 < ep < 1 (the smaller the better)
// gamma -> probability for error (the smaller the better) 0 < gamm < 1
CountMinSketch::CountMinSketch(float ep, float gamm) {
  if (!(0.009 <= ep && ep < 1)) {
    cout << "eps must be in this range: [0.01, 1)" << endl;
    exit(EXIT_FAILURE);
  } else if (!(0 < gamm && gamm < 1)) {
    cout << "gamma must be in this range: (0,1)" << endl;
    exit(EXIT_FAILURE);
  }
  eps = ep;
  gamma = gamm;
  w = ceil(exp(1)/eps);
  d = ceil(log(1/gamma));
  total = 0;
  // initialize counter array of arrays, C
  C = new int *[d];
  unsigned int i, j;
  for (i = 0; i < d; i++) {
    C[i] = new int[w];
    for (j = 0; j < w; j++) {
      C[i][j] = 0;
    }
  }
  // initialize d pairwise independent hashes
  srand(time(NULL));
  hashes = new int* [d];
  for (i = 0; i < d; i++) {
    hashes[i] = new int[2];
    genajbj(hashes, i);
  }
}

// CountMinSkectch destructor
CountMinSketch::~CountMinSketch() {
  // free array of counters, C
  unsigned int i;
  for (i = 0; i < d; i++) {
    delete[] C[i];
  }
  delete[] C;
  
  // free array of hash values
  for (i = 0; i < d; i++) {
    delete[] hashes[i];
  }
  delete[] hashes;
}

// CountMinSketch totalcount returns the
// total count of all items in the sketch
unsigned int CountMinSketch::totalcount() {
  return total;
}

// countMinSketch update item count (int)
void CountMinSketch::update(int item, int c) {
  total = total + c;
  unsigned int hashval = 0;
  for (unsigned int j = 0; j < d; j++) {
    hashval = (hashes[j][0]*item+hashes[j][1])%w;
    C[j][hashval] = C[j][hashval] + c;
  }
}

// countMinSketch update item count (string)
void CountMinSketch::update(const char *str, int c) {
  int hashval = hashstr(str);
  update(hashval, c);
}

// CountMinSketch estimate item count (int)
unsigned int CountMinSketch::estimate(int item) {
  int minval = numeric_limits<int>::max();
  unsigned int hashval = 0;
  for (unsigned int j = 0; j < d; j++) {
    hashval = (hashes[j][0]*item+hashes[j][1])%w;
    minval = MIN(minval, C[j][hashval]);
  }
  return minval;
}

// CountMinSketch estimate item count (string)
unsigned int CountMinSketch::estimate(const char *str) {
  int hashval = hashstr(str);
  return estimate(hashval);
}

// erase counts
void CountMinSketch::erase() {
  unsigned int i, j;
  for (i = 0; i < d; i++) {
    for (j = 0; j < w; j++) {
      C[i][j] = 0;
    }
  }
  total = 0; // zero the running total
}

// generates aj,bj from field Z_p for use in hashing
void CountMinSketch::genajbj(int** hashes, int i) {
  hashes[i][0] = int(float(rand())*float(LONG_PRIME)/float(RAND_MAX) + 1);
  hashes[i][1] = int(float(rand())*float(LONG_PRIME)/float(RAND_MAX) + 1);
}

// generates a hash value for a sting
// same as djb2 hash function
unsigned int CountMinSketch::hashstr(const char *str) {
  unsigned long hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return hash;
}

void CountMinSketch::swap(CountMinSketch& rhs)
{
  std::swap(eps, rhs.eps);
  std::swap(this->gamma, rhs.gamma);
  std::swap(total, rhs.total);
  std::swap(w, rhs.w);
  std::swap(d, rhs.d);
  std::swap(C, rhs.C);
  std::swap(hashes, rhs.hashes);
}

// XXX - this function does not currently convert everything to network byte order
void CountMinSketch::dump(std::ostream& os) const
{
  unsigned int i=0;
  os.write((char*)&eps, sizeof(eps));
  os.write((char*)&gamma, sizeof(gamma));
  os.write((char*)&total, sizeof(total));
  for (i = 0; i < d; i++) {
    os.write((char*)&(C[i][0]), sizeof(C[i][0])*w);
  }
  for (i = 0; i < d; i++) {
    os.write((char*)&(hashes[i][0]), sizeof(hashes[i][0])*2);
  }
  if(os.fail()){
    throw std::runtime_error("CountMinSketch: Failed to dump");
  }
}

// XXX - this function does not currently convert everything from network byte order
void CountMinSketch::restore(std::istream& is)
{
  float myeps=0;
  float mygamma=0;
  unsigned int i=0;
  is.read((char*)&myeps, sizeof(myeps));
  is.read((char*)&mygamma, sizeof(mygamma));
  CountMinSketch tempCMS(myeps, mygamma);
  is.read((char*)&tempCMS.total, sizeof(tempCMS.total));
  for (i = 0; i < tempCMS.d; i++) {
    is.read((char*)&(tempCMS.C[i][0]), sizeof(tempCMS.C[i][0])*tempCMS.w);
  }
  for (i = 0; i < tempCMS.d; i++) {
    is.read((char*)&(tempCMS.hashes[i][0]), sizeof(tempCMS.hashes[i][0])*2);
  }
  if(is.fail()){
    throw std::runtime_error("CountMinSketch: Failed to restore");
  }
  swap(tempCMS);
}
