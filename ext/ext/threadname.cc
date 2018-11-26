/*
 * This file is part of PowerDNS or dnsdist.
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
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pthread.h>

#if HAVE_PTHREAD_NP_H
#include <pthread_np.h>
#endif

#include "threadname.hh"

void setThreadName(const std::string& threadName) {
#ifdef HAVE_PTHREAD_SETNAME_NP_2
  pthread_setname_np(pthread_self(), threadName.c_str());
#endif
#ifdef HAVE_PTHREAD_SET_NAME_NP_2
  pthread_set_name_np(pthread_self(), threadName.c_str());
#endif
#ifdef HAVE_PTHREAD_SET_NAME_NP_2_VOID
  pthread_set_name_np(pthread_self(), threadName.c_str());
#endif
#ifdef HAVE_PTHREAD_SETNAME_NP_1
  pthread_setname_np(threadName.c_str());
#endif
#ifdef HAVE_PTHREAD_SETNAME_NP_3
  pthread_setname_np(pthread_self(), threadName.c_str(), nullptr);
#endif
}

