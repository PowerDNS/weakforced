#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_NO_MAIN
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <boost/test/unit_test.hpp>

#include "minicurl.hh"

BOOST_AUTO_TEST_SUITE(test_minicurl)

BOOST_AUTO_TEST_CASE(test_mini_simple) {
  MiniCurl mc;

  mc.setID(1234);
  BOOST_CHECK(mc.getID() == 1234);
}

BOOST_AUTO_TEST_SUITE_END();
