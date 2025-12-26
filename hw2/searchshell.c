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

// Feature test macro for strtok_r (c.f., Linux Programming Interface p. 63)
#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "libhw1/CSE333.h"
#include "./CrawlFileTree.h"
#include "./DocTable.h"
#include "./MemIndex.h"

//////////////////////////////////////////////////////////////////////////////
// Helper function declarations, constants, etc
static void Usage(void);
static void ProcessQueries(DocTable *dt, MemIndex *mi);
static int GetNextLine(FILE *f, char **ret_str);


//////////////////////////////////////////////////////////////////////////////
// Main
int main(int argc, char **argv) {
  if (argc != 2) {
    Usage();
  }

  // searchshell implementation:
  //
  //  - Crawl from a directory provided by argv[1] to produce and index
  //  - Prompt the user for a query and read the query from stdin, in a loop
  //  - Split a query into words (check out strtok_r)
  //  - Process a query against the index and print out the results
  //
  // When searchshell detects end-of-file on stdin, searchshell should free
  // all dynamically allocated memory and any other allocated resources and
  // then exit.

  // crawl directory in argv[1]
  DocTable *dt;
  MemIndex *mi;

  printf("Indexing '%s'\n", argv[1]);
  fflush(stdout);

  if (!CrawlFileTree(argv[1], &dt, &mi)) {
    fprintf(stderr, "crawl failed: %s\n", argv[1]);
    exit(EXIT_FAILURE);
  }

  ProcessQueries(dt, mi);

  DocTable_Free(dt);
  MemIndex_Free(mi);

  return EXIT_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////
// Helper function definitions

static void Usage(void) {
  fprintf(stderr, "Usage: ./searchshell <docroot>\n");
  fprintf(stderr,
          "where <docroot> is an absolute or relative " \
          "path to a directory to build an index under.\n");
  exit(EXIT_FAILURE);
}

static void ProcessQueries(DocTable *dt, MemIndex *mi) {
  char *line = NULL;
  // prompt user for query
  while (1) {
    printf("enter query:\n");
    fflush(stdout);
    // read query from stdin
    if (GetNextLine(stdin, &line) == -1) {
      printf("shutting down...\n");
      fflush(stdout);
      break;
    }

    // replace newline char with null terminator
    size_t len = strlen(line);
    if (len > 0 && line[strlen(line) - 1] == '\n') {
      line[len - 1] = '\0';
    }

    // split query into words using strtok_r to tokenize query
    char *saveptr;
    char *token = strtok_r(line, " \t", &saveptr);
    if (token == NULL) {
      free(line);
      continue;
    }

    // organizing tokens into a query array
    char *query[1024];
    int word = 0;
    while (token != NULL && word < 1024) {
      // convert to lowercase
      for (char *p = token; *p != '\0'; p++) {
        *p = (char) tolower(*p);
      }

      query[word++] = token;
      token = strtok_r(NULL, " \t", &saveptr);
    }

    // edge case of blank line input
    if (word == 0) {
      free(line);
      continue;
    }

    // process query against memindex
    // "results" holds the docs that match
    LinkedList *results = MemIndex_Search(mi, query, word);
    if (results == NULL) {
      free(line);
      continue;
    }

    // and print the results
    // user ll_itr to access what we need to print out for each matching doc:
    // path and rank
    LLIterator *itr = LLIterator_Allocate(results);
    while (LLIterator_IsValid(itr)) {
      SearchResult *sr;
      LLIterator_Get(itr, (LLPayload_t*) &sr);
      char *path_name = DocTable_GetDocName(dt, sr->doc_id);
      printf("  %s (%d)\n", path_name, sr->rank);
      LLIterator_Next(itr);
    }

    // cleanup resources used
    LLIterator_Free(itr);
    LinkedList_Free(results, &free);
    free(line);
  }
}

static int GetNextLine(FILE *f, char **ret_str) {
  if (f == NULL || ret_str == NULL) {
    return -1;
  }

  size_t bufsize = 128;
  size_t len = 0;  // len of current line
  int c;  // current character

  char *buf = malloc(bufsize);
  if (buf == NULL) {
    return -1;
  }

  while ((c = fgetc(f)) != EOF) {
    // buf resizing behavior
    if (len + 1 >= bufsize) {
      bufsize *= 2;
      char *newbuf = realloc(buf, bufsize);
      if (newbuf == NULL) {
        free(buf);
        return -1;
      }
      buf = newbuf;
    }

    // adds each character to the buf until we reach a newline
    buf[len++] = (char) c;
    if (c == '\n') {
      break;
    }
  }

  // edge case for EOF + no chars read (fail)
  if (len == 0 && c == EOF) {
    free(buf);
    return -1;
  }

  // return line thru ret_str parameter
  buf[len] = '\0';
  *ret_str = buf;
  return (int) len;
}
