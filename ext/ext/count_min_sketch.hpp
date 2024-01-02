/** 
    Author: Daniel Alabi
    http://alabidan.me
    GitHub: alabid/countminsketch
    Count-Min Sketch Implementation based on paper by
    Muthukrishnan and Cormode, 2004
**/

#pragma once

// define some constants
# define LONG_PRIME 32993
#ifndef MIN
# define MIN(a,b)  (a < b ? a : b)
#endif

  /** CountMinSketch class definition here **/
class CountMinSketch {
  protected:
    // width, depth
    unsigned int w,d;
  
    // eps (for error), 0.01 < eps < 1
    // the smaller the better
    float eps;
  
    // gamma (probability for accuracy), 0 < gamma < 1
    // the bigger the better
    float gamma;
  
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

    // Exchange the contents of an instance
    void swap(CountMinSketch& rhs);

    // Dump to a stream
    void dump(std::ostream& os) const;

    // Restore from a stream
    void restore(std::istream& is);

    // destructor
    ~CountMinSketch();
  };
