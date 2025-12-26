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

// Feature test macro enabling strdup (c.f., Linux Programming Interface p. 63)
#define _XOPEN_SOURCE 600

#include "./FileParser.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#include "libhw1/CSE333.h"
#include "./MemIndex.h"


///////////////////////////////////////////////////////////////////////////////
// Constants and declarations of internal-only helpers
///////////////////////////////////////////////////////////////////////////////
#define ASCII_UPPER_BOUND 0x7F

// Since our hash table dynamically grows, we'll start with a small
// number of buckets.
#define HASHTABLE_INITIAL_NUM_BUCKETS 2

// Frees a WordPositions.positions's payload, which is just a
// DocPositionOffset_t.
static void NoOpFree(LLPayload_t payload) { }

// Frees a WordPositions struct.
static void FreeWordPositions(HTValue_t payload) {
  WordPositions *pos = (WordPositions*) payload;
  LinkedList_Free(pos->positions, &NoOpFree);
  free(pos->word);
  free(pos);
}

// Add a normalized word and its byte offset into the WordPositions HashTable.
static void AddWordPosition(HashTable *tab, char *word,
                            DocPositionOffset_t pos);

// Parse the passed-in string into normalized words and insert into a HashTable
// of WordPositions structures.
static void InsertContent(HashTable *tab, char *content);


///////////////////////////////////////////////////////////////////////////////
// Publically-exported functions
///////////////////////////////////////////////////////////////////////////////
char* ReadFileToString(const char *file_name, int *size) {
  struct stat file_stat;
  char *buf;
  int result, fd;
  size_t left_to_read;

  if (stat(file_name, &file_stat) == -1) {
    perror("stat failed");
    return NULL;
  }

  // make sure this is a "regular file" and not a directory or something else
  if (!S_ISREG(file_stat.st_mode)) {
      fprintf(stderr, "not a regular file");
      return NULL;
  }

  // Attempt to open the file for reading
  fd = open(file_name, O_RDONLY);
  if (fd == -1) {
    perror("open failed");
    return NULL;
  }

  // Allocate space for the file, plus 1 extra byte to
  // '\0'-terminate the string.
  buf = (char*) malloc((size_t)file_stat.st_size + 1);
  if (buf == NULL) {
    perror("malloc failed");
    close(fd);
    return NULL;
  }

  // read in the file contents
  left_to_read = (size_t)file_stat.st_size;
  while (left_to_read > 0) {
    result = read(fd, buf + (file_stat.st_size - left_to_read), left_to_read);
    if (result == -1) {
      if (errno != EINTR && errno != EAGAIN) {
        perror("read failed: non-recoverable error");
        free(buf);
        close(fd);
        return NULL;
      }
      continue;
    } else if (result == 0) {
      break;
    }
    left_to_read -= result;
  }

  // Great, we're done!  We hit the end of the file and we read
  // filestat.st_size - left_to_read bytes. Close the file descriptor returned
  // by open() and return through the "size" output parameter how many bytes
  // we read.
  close(fd);
  *size = file_stat.st_size - left_to_read;

  // Null-terminate the string.
  buf[*size] = '\0';
  return buf;
}

HashTable* ParseIntoWordPositionsTable(char *file_contents) {
  HashTable *tab;
  int i, file_len;

  if (file_contents == NULL) {
    return NULL;
  }

  file_len = strlen(file_contents);
  if (file_len == 0) {
    return NULL;
  }

  // Verify that the file contains only ASCII text.  We won't try to index any
  // files that contain non-ASCII text; unfortunately, this means we aren't
  // Unicode friendly.
  for (i = 0; i < file_len; i++) {
    if (file_contents[i] == '\0' ||
        (unsigned char) file_contents[i] > ASCII_UPPER_BOUND) {
      free(file_contents);
      return NULL;
    }
  }

  // Great!  Let's split the file up into words.  We'll allocate the hash
  // table that will store the WordPositions structures associated with
  // each word.
  tab = HashTable_Allocate(HASHTABLE_INITIAL_NUM_BUCKETS);
  Verify333(tab != NULL);

  // Loop through the file, splitting it into words and inserting a record for
  // each word.
  InsertContent(tab, file_contents);

  // If we found no words, return NULL instead of a zero-sized hashtable.
  if (HashTable_NumElements(tab) == 0) {
    HashTable_Free(tab, &FreeWordPositions);
    tab = NULL;
  }

  // Now that we've finished parsing the document, we can free up the
  // filecontents buffer and return our built-up table.
  free(file_contents);
  return tab;
}

void FreeWordPositionsTable(HashTable *table) {
  HashTable_Free(table, &FreeWordPositions);
}


///////////////////////////////////////////////////////////////////////////////
// Internal helper functions

static void InsertContent(HashTable *tab, char *content) {
  char *cur_ptr = content,
    *word_start = content;

  // steps through the file content one character at a time, testing to see
  // whether a character is an alphabetic character. if a character is
  // alphabetic, it's part of a word. if a character is not alphabetic, it's
  // part of the boundary between words

  bool in_word = false;
  while (*cur_ptr != '\0') {
    // if character is alphabetical, lowercase it
    // if character is not in a word, then adjust word_start
    // if character is in a word, carry on
    if (isalpha((unsigned char) *cur_ptr)) {
      *cur_ptr = tolower((unsigned char) *cur_ptr);
      if (!in_word) {
        word_start = cur_ptr;
        in_word = true;
      }
    // if character is a boundary character
    // if character is in a word, end the word, record it
    // regardless of in_word, overwrite boundary character with null terminator
    } else {
      if (in_word) {
        in_word = false;
        *cur_ptr = '\0';
        AddWordPosition(tab, word_start, (int)(word_start - content));
      }
      *cur_ptr = '\0';
    }
    cur_ptr++;
  }

  // edge case: last character of text was part of a word
  if (in_word) {
    AddWordPosition(tab, word_start, (int)(word_start - content));
  }
}

static void AddWordPosition(HashTable *tab, char *word,
                            DocPositionOffset_t pos) {
  HTKey_t hash_key;
  HTKeyValue_t kv;
  WordPositions *wp;

  // Hash the string.
  hash_key = FNVHash64((unsigned char*) word, strlen(word));

  // Have we already encountered this word within this file?  If so, it's
  // already in the hashtable.
  if (HashTable_Find(tab, hash_key, &kv)) {
    // Yes; we just need to add a position in using LinkedList_Append(). Note
    // how we're casting the DocPositionOffset_t position variable to an
    // LLPayload_t to store it in the linked list payload without needing to
    // malloc space for it.  Ugly, but it works!
    wp = (WordPositions*) kv.value;

    // Ensure we don't have hash collisions (two different words that hash to
    // the same key, which is very unlikely).
    Verify333(strcmp(wp->word, word) == 0);

    LinkedList_Append(wp->positions, (LLPayload_t) (int64_t) pos);
  } else {
    // No; this is the first time we've seen this word.  Allocate and prepare
    // a new WordPositions structure, and append the new position to its list
    // using a similar ugly hack as right above.
    wp = (WordPositions*) malloc(sizeof(WordPositions));
    if (wp == NULL) {
      perror("malloc failed");
      exit(EXIT_FAILURE);
    }
    wp->word = strdup(word);
    wp->positions = LinkedList_Allocate();
    LinkedList_Append(wp->positions, (LLPayload_t) (int64_t) pos);
    kv.key = hash_key;
    kv.value = wp;
    HTKeyValue_t oldkv;
    if (HashTable_Insert(tab, kv, &oldkv)) {
      FreeWordPositions(oldkv.value);
    }
  }
}
