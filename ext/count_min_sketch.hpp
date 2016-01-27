/** 
    Author: Daniel Alabi
    http://alabidan.me
    Github: alabid/countminsketch
    Count-Min Sketch Implementation based on paper by
    Muthukrishnan and Cormode, 2004
**/

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

// define some constants
# define LONG_PRIME 32993
# define MIN(a,b)  (a < b ? a : b)

/** CountMinSketch class definition here **/
class CountMinSketch {
private:
  friend class boost::serialization::access;

  template <typename Archive>
  void serialize(Archive &ar, const unsigned int version) { 
    ar & w; 
    ar & d; 
    ar & eps; 
    ar & gamma; 
    ar & aj; 
    ar & bj; 
    ar & total; 
    for (unsigned int i = 0; i < d; i++) {
      for (unsigned int j = 0; j < w; j++) {
	ar & C[i][j];
      }
    }
    for (unsigned int i = 0; i < d; i++) {
      ar & hashes[i][0];
      ar & hashes[i][1];
    }
  }    

  // width, depth 
  unsigned int w,d;
  
  // eps (for error), 0.01 < eps < 1
  // the smaller the better
  float eps;
  
  // gamma (probability for accuracy), 0 < gamma < 1
  // the bigger the better
  float gamma;
  
  // aj, bj \in Z_p
  // both elements of fild Z_p used in generation of hash
  // function
  unsigned int aj, bj;

  // total count so far
  unsigned int total; 

  // array of arrays of counters
  int **C;

  // array of hash values for a particular item 
  // contains two element arrays {aj,bj}
  int **hashes;

  // generate "new" aj,bj
  void genajbj(int **hashes, int i);

public:
  // constructor
  CountMinSketch();
  CountMinSketch(float eps, float gamma);
  
  // update item (int) by count c
  void update(int item, int c);
  // update item (string) by count c
  void update(const char *item, int c);

  // estimate count of item i and return count
  unsigned int estimate(int item);
  unsigned int estimate(const char *item);

  // return total count
  unsigned int totalcount();

  // generates a hash value for a string
  // same as djb2 hash function
  unsigned int hashstr(const char *str);

  // erase counts
  void erase();

  // destructor
  ~CountMinSketch();
};


