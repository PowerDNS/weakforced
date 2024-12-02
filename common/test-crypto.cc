#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_NO_MAIN
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <boost/test/unit_test.hpp>
#include "sodcrypto.hh"

BOOST_AUTO_TEST_SUITE(test_crypto)

  BOOST_AUTO_TEST_CASE(test_symmetric)
  {
    std::string keyStr = newKeyStr();
    SodiumNonce wnonce, rnonce;
    wnonce.init();
    memcpy((char*)&rnonce, wnonce.value, sizeof(rnonce.value));
    std::string ciphertext, plaintext;
    std::string msg("Hello");
    ciphertext= sodEncryptSym(msg, keyStr, wnonce);
    plaintext= sodDecryptSym(ciphertext, keyStr, rnonce);
    BOOST_CHECK(plaintext == msg);
  }

BOOST_AUTO_TEST_SUITE_END();
