/*
 * This file is part of PowerDNS or weakforced.
 * Copyright -- PowerDNS.COM B.V. and its contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * In addition, for the avoidance of any doubt, permission is granted to
 * link this program with OpenSSL and to (re)distribute the binaries
 * produced as the result of such linking.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once
#include "config.h"
#include <string>
#include <stdint.h>

#include <arpa/inet.h>

#ifndef HAVE_LIBSODIUM
struct SodiumNonce
{
  void init(){};
  void merge(const SodiumNonce& lower, const SodiumNonce& higher){};
  void increment(){};
  std::string toString() { return string(""); }
  unsigned char value[1];
};
#define crypto_secretbox_NONCEBYTES 0
#else
#include <sodium.h>

struct SodiumNonce
{
  void init()
  {
    randombytes_buf(value, sizeof value);
  }

  void merge(const SodiumNonce& lower, const SodiumNonce& higher)
  {
    static const size_t halfSize = (sizeof value) / 2;
    memcpy(value, lower.value, halfSize);
    memcpy(value + halfSize, higher.value + halfSize, halfSize);
  }
  
  void increment()
  {
    uint32_t* p = (uint32_t*)value;
    uint32_t count=htonl(*p);
    ++count;
    *p=ntohl(count);
  }

  string toString() const
  {
    return string((const char*)value, crypto_secretbox_NONCEBYTES);
  }

  unsigned char value[crypto_secretbox_NONCEBYTES];
};
#endif
std::string newKeypair();
std::string sodEncryptSym(const std::string& msg, const std::string& key, SodiumNonce&);
std::string sodDecryptSym(const std::string& msg, const std::string& key, SodiumNonce&);
std::string newKey();
