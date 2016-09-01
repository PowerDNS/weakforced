/*
 * This file is part of PowerDNS or weakforced.
 * Copyright -- PowerDNS.COM B.V. and its contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
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

#include <string>
#include "hmac.hh"
#include "wforce_exception.hh"

static const EVP_MD* getMDType(HashAlgo algo)
{
  switch(algo) {
  case HashAlgo::SHA224:
    return EVP_sha224();
    break;
  case HashAlgo::SHA256:
    return EVP_sha256();
    break;
  case HashAlgo::SHA384:
    return EVP_sha384();
    break;
  case HashAlgo::SHA512:
    return EVP_sha512();
    break;
  default:
    throw WforceException("Unknown hash algorithm requested from calculateHMAC()");
  }
}

// This function is borrowed and modified from pdns src
std::string calculateHMAC(const std::string& key, const std::string& text, HashAlgo algo)
{
  const EVP_MD* md_type;
  unsigned int outlen;
  unsigned char hash[EVP_MAX_MD_SIZE];

  md_type = getMDType(algo);
  
  unsigned char* out = HMAC(md_type,
			    reinterpret_cast<const unsigned char*>(key.c_str()), key.size(),
			    reinterpret_cast<const unsigned char*>(text.c_str()), text.size(),
			    hash, &outlen);
  if (out != NULL && outlen > 0) {
    return std::string((char*) hash, outlen);
  }

  return "";
}

std::string calculateHash(const std::string& text, HashAlgo algo)
{
  EVP_MD_CTX *mdctx;
  const EVP_MD *md_type;
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int out_len;

  md_type = getMDType(algo);

  mdctx = EVP_MD_CTX_create();
  EVP_DigestInit_ex(mdctx, md_type, NULL);
  EVP_DigestUpdate(mdctx, reinterpret_cast<const unsigned char*>(text.c_str()), text.size());
  int ret = EVP_DigestFinal_ex(mdctx, hash, &out_len);
  EVP_MD_CTX_destroy(mdctx);

  if ((ret == 1) && (out_len > 0)) {
    return std::string((char*) hash, out_len);
  }

  return "";
}
