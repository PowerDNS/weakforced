#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE unit

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <boost/test/unit_test.hpp>
#include "dolog.hh"

bool g_console = false;
bool g_docker = false;
LogLevel g_loglevel{LogLevel::Info};
