/*
 * Copyright ©2025 Chris Thachuk & Naomi Alterman.  All rights reserved.
 * Permission is hereby granted to students registered for University of
 * Washington CSE 333 for use solely during Autumn Quarter 2025 for
 * purposes of the course.  No other use, copying, distribution, or
 * modification is permitted without prior written consent. Copyrights
 * for third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

// This file contains a number of HTTP and HTML parsing routines
// that come in useful throughput the assignment.

#include "./HttpUtils.h"

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string.hpp>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <utility>

using boost::algorithm::replace_all;
using std::cerr;
using std::endl;
using std::map;
using std::pair;
using std::string;
using std::vector;

namespace hw4 {

bool IsPathSafe(const string &root_dir, const string &test_file) {

  char root_real[PATH_MAX], file_real[PATH_MAX];
  // get rootdir canonical pathname
  if (!realpath(root_dir.c_str(), root_real)) {
    return false;  // doesnt exist
  }
  // get testfile canonical pathname
  if (!realpath(test_file.c_str(), file_real)) {
    return false;  // doesnt exist
  }

  std::string root(root_real), file(file_real);
  // root path cannot be longer than file path
  if (root.size() > file.size()) {
    return false;  // invalid filepath
  }
  // compare prefixes, ensure filepath starts with root
  if (file.compare(0, root.size(), root) != 0) {
    return false;  // invalid filepath
  }

  // either exact match or next character is '/'
  if (file.size() == root.size()) {
    return true;
  }
  return file[root.size()] == '/';
}

string EscapeHtml(const string &from) {
  string ret = from;

  // unsafe characters: &, <, >, ", '
  boost::algorithm::replace_all(ret, "&", "&amp;");
  boost::algorithm::replace_all(ret, "<", "&lt;");
  boost::algorithm::replace_all(ret, ">", "&gt;");
  boost::algorithm::replace_all(ret, "\"", "&quot;");
  boost::algorithm::replace_all(ret, "\'", "&apos;");

  return ret;
}

// Look for a "%XY" token in the string, where XY is a
// hex number.  Replace the token with the appropriate ASCII
// character, but only if 32 <= dec(XY) <= 127.
string URIDecode(const string &from) {
  string retstr;

  // Loop through the characters in the string.
  for (unsigned int pos = 0; pos < from.length(); pos++) {
    // note: use pos+n<from.length() instead of pos<from.length-n
    // to avoid overflow problems with unsigned int values
    char c1 = from[pos];
    char c2 = (pos+1 < from.length()) ? toupper(from[pos+1]) : ' ';
    char c3 = (pos+2 < from.length()) ? toupper(from[pos+2]) : ' ';

    // Special case the '+' for old encoders.
    if (c1 == '+') {
      retstr.append(1, ' ');
      continue;
    }

    // Is this an escape sequence?
    if (c1 != '%') {
      retstr.append(1, c1);
      continue;
    }

    // Yes.  Are the next two characters hex digits?
    if (!((('0' <= c2) && (c2 <= '9')) ||
          (('A' <= c2) && (c2 <= 'F')))) {
      retstr.append(1, c1);
      continue;
    }
    if (!((('0' <= c3) && (c3 <= '9')) ||
           (('A' <= c3) && (c3 <= 'F')))) {
      retstr.append(1, c1);
      continue;
    }

    // Yes.  Convert to an index, which we can use in the
    // ascii table (aka a "code").
    int8_t code = 0;
    if (c2 >= 'A') {
      code = 16 * (10 + (c2 - 'A'));
    } else {
      code = 16 * (c2 - '0');
    }
    if (c3 >= 'A') {
      code += 10 + (c3 - 'A');
    } else {
      code += (c3 - '0');
    }

    // Is the code reasonable?
    if (code < 32 || code > 127) {
      retstr.append(1, c1);
      continue;
    }

    // Great!  Convert and append.
    retstr.append(1, code);
    pos += 2;
  }
  return retstr;
}

void URLParser::Parse(const string &url) {
  url_ = url;

  // Split the URL into the path and the args components.
  vector<string> ps;
  boost::split(ps, url, boost::is_any_of("?"));
  if (ps.size() < 1)
    return;

  // Store the URI-decoded path.
  path_ = URIDecode(ps[0]);

  if (ps.size() < 2)
    return;

  // Split the args into each field=val; chunk.
  vector<string> vals;
  boost::split(vals, ps[1], boost::is_any_of("&"));

  // Iterate through the chunks.
  for (unsigned int i = 0; i < vals.size(); i++) {
    // Split the chunk into field, value.
    string val = vals[i];
    vector<string> fv;
    boost::split(fv, val, boost::is_any_of("="));
    if (fv.size() == 2) {
      // Add the field, value to the args_ map.
      args_[URIDecode(fv[0])] = URIDecode(fv[1]);
    }
  }
}

uint16_t GetRandPort() {
  uint16_t portnum = 10000;
  portnum += ((uint16_t) getpid()) % 25000;
  portnum += ((uint16_t) rand()) % 5000;  // NOLINT(runtime/threadsafe_fn)
  return portnum;
}

int WrappedRead(int fd, unsigned char *buf, int read_len) {
  int res;
  while (1) {
    res = read(fd, buf, read_len);
    if (res == -1) {
      if ((errno == EAGAIN) || (errno == EINTR))
        continue;
    }
    break;
  }
  return res;
}

int WrappedWrite(int fd, const unsigned char *buf, int write_len) {
  int res, written_so_far = 0;

  while (written_so_far < write_len) {
    res = write(fd, buf + written_so_far, write_len - written_so_far);
    if (res == -1) {
      if ((errno == EAGAIN) || (errno == EINTR))
        continue;
      break;
    }
    if (res == 0)
      break;
    written_so_far += res;
  }
  return written_so_far;
}

bool ConnectToServer(const string &host_name, uint16_t port_num,
                     int *client_fd) {
  struct addrinfo hints;
  struct addrinfo *results;
  struct addrinfo *r;
  int client_sock, ret_val;
  char port_str[10];

  // Convert the port number to a C-style string.
  snprintf(port_str, sizeof(port_str), "%hu", port_num);

  // Zero out the hints data structure using memset.
  memset(&hints, 0, sizeof(hints));

  // Indicate we're happy with either AF_INET or AF_INET6 addresses.
  hints.ai_family = AF_UNSPEC;

  // Constrain the answers to SOCK_STREAM addresses.
  hints.ai_socktype = SOCK_STREAM;

  // Do the lookup.
  if ((ret_val = getaddrinfo(host_name.c_str(),
                            port_str,
                            &hints,
                            &results)) != 0) {
    cerr << "getaddrinfo failed: ";
    cerr << gai_strerror(ret_val) << endl;
    return false;
  }

  // Loop through, trying each out until one succeeds.
  for (r = results; r != nullptr; r = r->ai_next) {
    // Try manufacturing the socket.
    if ((client_sock = socket(r->ai_family, SOCK_STREAM, 0)) == -1) {
      continue;
    }
    // Try connecting to the peer.
    if (connect(client_sock, r->ai_addr, r->ai_addrlen) == -1) {
      continue;
    }
    *client_fd = client_sock;
    freeaddrinfo(results);
    return true;
  }
  freeaddrinfo(results);
  return false;
}

}  // namespace hw4
