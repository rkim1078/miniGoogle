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

#include <stdio.h>
#include <string>
#include <cstdlib>
#include <iostream>
#include <sstream>

extern "C" {
  #include "libhw2/FileParser.h"
}

#include "./HttpUtils.h"
#include "./FileReader.h"

using std::string;

namespace hw4 {

bool FileReader::ReadFile(string *const contents) {
  string full_file = basedir_ + "/" + fname_;

  // Read the file into memory, and store the file contents in the
  // output parameter "contents."
  
  // make sure requested file lives in basedir_
  if (!IsPathSafe(basedir_, full_file)) {
    return false;
  }

  // read file into memory, store in contents
  int size = 0;
  char *buf = ReadFileToString(full_file.c_str(), &size);
  if (buf == nullptr) {
    return false;  // failed to open/read
  }

  // binary safe handling of data
  *contents = std::string(buf, size);

  free(buf);
  return true;
}

}  // namespace hw4
