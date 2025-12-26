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

#include "./QueryProcessor.h"

#include <iostream>
#include <algorithm>
#include <list>
#include <string>
#include <vector>
#include <map>

extern "C" {
  #include "./libhw1/CSE333.h"
}

using std::list;
using std::sort;
using std::string;
using std::vector;

namespace hw3 {

QueryProcessor::QueryProcessor(const list<string> &index_list, bool validate) {
  // Stash away a copy of the index list.
  index_list_ = index_list;
  array_len_ = index_list_.size();
  Verify333(array_len_ > 0);

  // Create the arrays of DocTableReader*'s. and IndexTableReader*'s.
  dtr_array_ = new DocTableReader*[array_len_];
  itr_array_ = new IndexTableReader*[array_len_];

  // Populate the arrays with heap-allocated DocTableReader and
  // IndexTableReader object instances.
  list<string>::const_iterator idx_iterator = index_list_.begin();
  for (int i = 0; i < array_len_; i++) {
    FileIndexReader fir(*idx_iterator, validate);
    dtr_array_[i] = fir.NewDocTableReader();
    itr_array_[i] = fir.NewIndexTableReader();
    idx_iterator++;
  }
}

QueryProcessor::~QueryProcessor() {
  // Delete the heap-allocated DocTableReader and IndexTableReader
  // object instances.
  Verify333(dtr_array_ != nullptr);
  Verify333(itr_array_ != nullptr);
  for (int i = 0; i < array_len_; i++) {
    delete dtr_array_[i];
    delete itr_array_[i];
  }

  // Delete the arrays of DocTableReader*'s and IndexTableReader*'s.
  delete[] dtr_array_;
  delete[] itr_array_;
  dtr_array_ = nullptr;
  itr_array_ = nullptr;
}

// This structure is used to store a index-file-specific query result.
typedef struct {
  DocID_t doc_id;  // The document ID within the index file.
  int rank;        // The rank of the result so far.
} IdxQueryResult;

vector<QueryProcessor::QueryResult>
QueryProcessor::ProcessQuery(const vector<string> &query) const {
  Verify333(query.size() > 0);

  vector<QueryProcessor::QueryResult> final_result;
  if (query.empty()) {
    return final_result;
  }

  // loop over all index files
  for (int i = 0; i < array_len_; i++) {
    // mapping from docID to rank for current index, tracks # of docID matches
    // per query word
    std::map<DocID_t, int> doc_ranks;
    // readers for this index file
    IndexTableReader* itr = itr_array_[i];
    DocTableReader* dtr = dtr_array_[i];

    DocIDTableReader* ditr = itr->LookupWord(query[0]);  // lookup first word
    if (!ditr) {
      // if first word isnt found, skip this index file
      continue;
    }

    // get list of docIDs that contain this first word
    list<DocIDElementHeader> result = ditr->GetDocIDList();
    delete ditr;

    // for multi-word queries, find intersection of current docID list w/ each
    // word after it
    for (size_t j = 1; j < query.size(); j++) {
      DocIDTableReader* ditrj = itr->LookupWord(query[j]);  // similar lookup
      if (!ditrj) {
        // if a word isnt found, then it's impossible for all words to be found
        result.clear();
        break;
      }

      // get list of docIDs that contain this query word
      list<DocIDElementHeader> next = ditrj->GetDocIDList();
      delete ditrj;

      // intersect the current result with the new list:
      // keep only docIDs that appear in both, and sum their position counts
      list<DocIDElementHeader> intersection;
      for (const auto& x : result) {
        for (const auto& y : next) {
          if (x.doc_id == y.doc_id) {
            // if docIDs show up in both lists, sum their ranks
            intersection.push_back({x.doc_id, x.num_positions +
              y.num_positions});
            break;
          }
        }
      }
      // update result
      result = intersection;
    }

    // convert docIDs to filenames and put them into the final result
    for (const auto& entry : result) {
      string docname;
      if (dtr->LookupDocID(entry.doc_id, &docname)) {
        final_result.push_back({docname, entry.num_positions});
      }
    }
  }


  // Sort the final results.
  sort(final_result.begin(), final_result.end());
  return final_result;
}

}  // namespace hw3
