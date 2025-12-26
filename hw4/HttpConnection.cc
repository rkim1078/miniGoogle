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

#include <stdint.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <map>
#include <string>
#include <vector>

#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpConnection.h"

using std::map;
using std::string;
using std::vector;

namespace hw4 {

static const char *kHeaderEnd = "\r\n\r\n";
static const int kHeaderEndLen = 4;

bool HttpConnection::GetNextRequest(HttpRequest *const request) {

  // keeps reading until connection drops or found end of request header
  // if buffer_ already has a complete header, we just parse directly
  size_t pos = buffer_.find(kHeaderEnd);
  while (pos == string::npos) {  // while header end can't be found in buffer_
    unsigned char tmp[8192];  // read in large chunks each WrappedRead call
    int n = WrappedRead(fd_, tmp, sizeof(tmp));
    if (n == -1) {
      // fatal err
      return false;
    }
    if (n == 0) {
      // stop reading case 1: connection dropped
      return false;
    }

    // append newly read bytes from file into buffer_
    buffer_.append(reinterpret_cast<char*>(tmp), n);

    // try again
    pos = buffer_.find(kHeaderEnd);
  }

  // extract just the relevant part of header from buffer_
  string header = buffer_.substr(0, pos + kHeaderEndLen);

  // get rid of processed header from buffer_, saving rest for b2b requests
  buffer_.erase(0, pos + kHeaderEndLen);

  // parse into an HttpRequest and save to the output param
  *request = ParseRequest(header);
  return true;
}

bool HttpConnection::WriteResponse(const HttpResponse &response) const {
  // We use a reinterpret_cast<> to cast between unrelated pointer types, and
  // a static_cast<> to perform a conversion from an unsigned type to its
  // corresponding signed one.
  string str = response.GenerateResponseString();
  int res = WrappedWrite(fd_,
                         reinterpret_cast<const unsigned char*>(str.c_str()),
                         str.length());

  if (res != static_cast<int>(str.length()))
    return false;
  return true;
}

HttpRequest HttpConnection::ParseRequest(const string &request) const {
  HttpRequest req("/");  // by default, get "/".

  // split request into lines on "\r\n"
  vector<string> lines;
  boost::split(lines, request, boost::is_any_of("\r\n"));
  if (lines.empty()) {
    return req;
  }

  // parse first line, should look like: "GET /path HTTP/1.1"
  vector<string> parts;
  boost::split(parts, lines[0], boost::is_any_of(" "));  // extract URI
  // make sure tokens are valid
  if (parts.size() >= 2 && boost::iequals(parts[0], "GET")) {
    req.set_uri(parts[1]);  // store in req.URI
  }

  // rest of lines: track header name and value and store them properly
  for (size_t i = 1; i < lines.size(); i++) {
    const string &line = lines[i];
    if (line.empty()) {
      continue;
    }

    size_t colon = line.find(':');
    if (colon == string::npos) {
      continue;  // bad format, skip
    }

    string name = line.substr(0, colon);  // header name
    string value = line.substr(colon + 1);  // and value
    boost::algorithm::trim(name);
    boost::algorithm::trim(value);

    // store in req
    // note: didn't use boost::algorithm::to_lower_copy
    // since AddHeader has tolower_copy
    if (!name.empty()) {
      req.AddHeader(name, value);
    }
  }

  return req;
}

}  // namespace hw4
