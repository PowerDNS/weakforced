#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_NO_MAIN
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <boost/test/unit_test.hpp>

#include "ext/hyperloglog.hpp"
#include "ext/count_min_sketch.hpp"
#include "misc.hh"

#include <iostream>

using namespace std;

BOOST_AUTO_TEST_SUITE(test_serialize)

BOOST_AUTO_TEST_CASE(test_serialize_hll) {
  hll::HyperLogLog hll(6);
  std::string foo="foo";
  std::string bar="bar";
  
  hll.add(foo.c_str(), foo.length());
  hll.add(bar.c_str(), bar.length());
  BOOST_CHECK((int)hll.estimate() == 2);

  std::stringstream ss(std::stringstream::in|std::stringstream::out|std::stringstream::binary);
  hll.dump(ss);

  hll::HyperLogLog hllcopy(6);
  hllcopy.restore(ss);

  BOOST_CHECK((int)hllcopy.estimate() == 2);
}

BOOST_AUTO_TEST_CASE(test_serialize_countmin) {
  CountMinSketch cm(0.05, 0.2);
  std::string foo="foo";
  std::string bar="bar";
  
  cm.update(foo.c_str(), 2);
  cm.update(bar.c_str(), 1);
  BOOST_CHECK((int)cm.estimate(foo.c_str()) == 2);

  // check we can dump and restore
  std::stringstream ss(std::stringstream::in|std::stringstream::out|std::stringstream::binary);
  cm.dump(ss);
  CountMinSketch cmcopy(0.05, 0.2);
  cmcopy.restore(ss);
  BOOST_CHECK((int)cmcopy.estimate(foo.c_str()) == 2);

  // check we can dump the copy and restore from that
  std::stringstream ss2(std::stringstream::in|std::stringstream::out|std::stringstream::binary);
  cmcopy.dump(ss2);
  CountMinSketch cmcopy2(0.05, 0.2);
  cmcopy2.restore(ss2);

  BOOST_CHECK((int)cmcopy2.estimate(foo.c_str()) == 2);
}

BOOST_AUTO_TEST_SUITE_END();
