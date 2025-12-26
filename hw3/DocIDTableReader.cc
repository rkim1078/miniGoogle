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

#include "./DocIDTableReader.h"

#include <list>      // for std::list

extern "C" {
  #include "libhw1/CSE333.h"
}
#include "./LayoutStructs.h"

using std::list;

namespace hw3 {

DocIDTableReader::DocIDTableReader(FILE *f, IndexFileOffset_t offset)
  : HashTableReader(f, offset) { }

bool DocIDTableReader::LookupDocID(
     const DocID_t &doc_id, list<DocPositionOffset_t> *const ret_val) const {
  // walk through the docIDtable and get back a list of offsets
  // to elements in the bucket for this docID.
  auto elements = LookupElementPositions(doc_id);

  // If the list of elements is empty, we're done.
  if (elements.empty())
    return false;

  // Iterate through all of elements, looking for our docID.
  for (IndexFileOffset_t &curr : elements) {
    // get the next docid out of the current element.
    DocIDElementHeader curr_header;
    Verify333(fseek(file_, curr, SEEK_SET) == 0);
    Verify333(fread(&curr_header, sizeof(curr_header), 1, file_) == 1);
    curr_header.ToHostFormat();


    // Is it a match?
    if (curr_header.doc_id == doc_id) {
      // Yes! Extract the positions themselves, appending to
      // std::list<DocPositionOffset_t>.
      for (int i = 0; i < curr_header.num_positions; i++) {
        DocIDElementPosition pos;
        Verify333(fread(&pos, sizeof(pos), 1, file_) == 1);
        pos.ToHostFormat();
        // return pos list thru output param
        ret_val->push_back(pos.position);
      }
      return true;
    }
  }

  // We failed to find a matching docID, so return false.
  return false;
}

list<DocIDElementHeader> DocIDTableReader::GetDocIDList() const {
  // This will be our returned list of docIDs within this table.
  list<DocIDElementHeader> doc_id_list;

  // Go through *all* of the buckets of this hashtable, extracting
  // out the docids in each element and the number of word positions
  // for the each docid.
  for (int i = 0; i < header_.num_buckets; i++) {
    // Seek to the next BucketRecord.  The "offset_" member
    // variable stores the offset of this docid table within
    // the index file.
    IndexFileOffset_t bucket_rec_offset =
      offset_ + sizeof(BucketListHeader) + i * sizeof(BucketRecord);
    Verify333(fseek(file_, bucket_rec_offset, SEEK_SET) == 0);


    // Read in the chain length and bucket position fields from
    // the bucket_rec.
    BucketRecord bucket_rec;
    Verify333(fread(&bucket_rec, sizeof(bucket_rec), 1, file_) == 1);
    bucket_rec.ToHostFormat();


    // Sweep through the next bucket, iterating through each
    // chain element in the bucket.
    for (int j = 0; j < bucket_rec.chain_num_elements; j++) {
      // Seek to chain element's position field in the bucket header.
      Verify333(fseek(file_, bucket_rec.position
                             + j*sizeof(ElementPositionRecord), SEEK_SET) == 0);

      // Read the next element position from the bucket header.
      // and seek to the element itself.
      ElementPositionRecord element_pos;
      Verify333(fread(&element_pos, sizeof(element_pos), 1, file_) == 1);
      element_pos.ToHostFormat();
      Verify333(fseek(file_, element_pos.position, SEEK_SET) == 0);


      // Read in the docid and number of positions from the element.
      DocIDElementHeader element;
      Verify333(fread(&element, sizeof(element), 1, file_) == 1);
      element.ToHostFormat();


      // Append it to our result list.
      doc_id_list.push_back(element);
    }
  }

  // Done!  Return the result list.
  return doc_id_list;
}

}  // namespace hw3
