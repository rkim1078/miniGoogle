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

#include <cstdlib>    // for EXIT_SUCCESS, EXIT_FAILURE
#include <iostream>   // for std::cout, std::cerr, etc.
#include <sstream>    // for istringstream
#include <algorithm>  // for transform

#include "./QueryProcessor.h"

using std::cerr;
using std::endl;
using std::list;
using std::string;
using std::vector;
using hw3::QueryProcessor;
using std::cout;
using std::cin;
using std::istringstream;

// Error usage message for the client to see
// Arguments:
// - prog_name: Name of the program
static void Usage(char *prog_name);

// filesearchshell implementation details:
//
// The user must be able to run the program using a command like:
//
//   ./filesearchshell ./foo.idx ./bar/baz.idx /tmp/blah.idx [etc]
//
// i.e., to pass a set of filenames of indices as command line
// arguments. Then, your program needs to implement a loop where
// each loop iteration it:
//
//  (a) prints to the console a prompt telling the user to input the
//      next query.
//
//  (b) reads a white-space separated list of query words from
//      std::cin, converts them to lowercase, and constructs
//      a vector of c++ strings out of them.
//
//  (c) uses QueryProcessor.cc/.h's QueryProcessor class to
//      process the query against the indices and get back a set of
//      query results. 
//
//  (d) print the query results to std::cout in the format shown in
//      the transcript on the hw3 web page.
//
// quit out of the loop when std::cin experiences EOF.
// users should be able to type in an arbitrarily long query.
// deallocate all dynamically allocated memory on loop exit.

// helper method to build a list of index files from args passed
static list<string> BuildIndexFileList(int argc, char **argv);

// helper method to print out query results
static void PrintResults(const vector<QueryProcessor::QueryResult> &results);

int main(int argc, char **argv) {
  if (argc < 2) {
    Usage(argv[0]);
  }

  // build list of index file paths from args
  list<string> index_files = BuildIndexFileList(argc, argv);
  // part (c) only instantiate one qp object over lifetime of program
  QueryProcessor qp(index_files);

  while (1) {
    // part (a)
    cout << "Enter query:" << endl;

    // part (b)
    // read whitespace-separated list of query words from cin
    string line;
    getline(cin, line);
    // convert them to lowercase
    transform(line.begin(), line.end(), line.begin(), ::tolower);

    // exit on ctrl+d
    if (cin.eof()) {
      break;
    }

    // make vector of cpp strings out of them
    // only istringstream since we don't need to write to this stream
    istringstream stream(line);
    vector<string> query;
    string word;
    while (stream >> word) {
      query.push_back(word);
    }

    // ignore empty queries
    if (query.empty()) {
      continue;
    }

    // part (c)
    // use queryprocessor to process query against indices, get back a result
    vector<QueryProcessor::QueryResult> results = qp.ProcessQuery(query);

    // part (d)
    // print out well-formatted results
    PrintResults(results);
  }

  cout << endl;
  return EXIT_SUCCESS;
}

static list<string> BuildIndexFileList(int argc, char **argv) {
  list<string> index_files;
  for (int i = 1; i < argc; i++) {
    index_files.push_back(argv[i]);
  }
  return index_files;
}

static void PrintResults(const vector<QueryProcessor::QueryResult> &results) {
  if (results.empty()) {
    cout << "  [no results]" << endl;
  } else {
    for (const auto &r : results) {
      cout << "  " << r.document_name << " (" << r.rank << ")" << endl;
    }
  }
}

static void Usage(char *prog_name) {
  cerr << "Usage: " << prog_name << " [index files+]" << endl;
  exit(EXIT_FAILURE);
}
