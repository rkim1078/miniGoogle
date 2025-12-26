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
#include <stdlib.h>

#include "CSE333.h"
#include "LinkedList.h"
#include "LinkedList_priv.h"


///////////////////////////////////////////////////////////////////////////////
// LinkedList implementation.

LinkedList* LinkedList_Allocate(void) {
  // Allocate the linked list record.
  LinkedList *ll = (LinkedList *) malloc(sizeof(LinkedList));
  Verify333(ll != NULL);

  ll->num_elements = 0;
  ll->head = NULL;
  ll->tail = NULL;

  // Return our newly minted linked list.
  return ll;
}

void LinkedList_Free(LinkedList *list,
                     LLPayloadFreeFnPtr payload_free_function) {
  Verify333(list != NULL);
  Verify333(payload_free_function != NULL);

  // sweep thru the list and free all of the nodes and their payloads
  for (int i = 0; i < list->num_elements; i++) {
    LinkedListNode *curr = list->head;
    (*payload_free_function)(curr->payload);
    list->head = curr->next;
    free(curr);
  }

  // free the LinkedList
  free(list);
}

int LinkedList_NumElements(LinkedList *list) {
  Verify333(list != NULL);
  return list->num_elements;
}

void LinkedList_Push(LinkedList *list, LLPayload_t payload) {
  Verify333(list != NULL);

  // Allocate space for the new node.
  LinkedListNode *ln = (LinkedListNode *) malloc(sizeof(LinkedListNode));
  Verify333(ln != NULL);

  // Set the payload
  ln->payload = payload;

  // Degenerate case; list is currently empty
  if (list->num_elements == 0) {
    Verify333(list->head == NULL);
    Verify333(list->tail == NULL);
    ln->next = ln->prev = NULL;
    list->head = list->tail = ln;
  } else {  // typical case; list has >=1 elements
    ln->next = list->head;
    ln->prev = NULL;
    list->head->prev = ln;
    list->head = ln;
  }
  list->num_elements += 1;
}

bool LinkedList_Pop(LinkedList *list, LLPayload_t *payload_ptr) {
  Verify333(payload_ptr != NULL);
  Verify333(list != NULL);

  // test for empty list
  if (list->num_elements == 0) {
    return false;
  }

  LinkedListNode *remove = list->head;
  *payload_ptr = remove->payload;

  // case (a) list with single element
  if (list->num_elements == 1) {
    list->head = list->tail = NULL;
  } else {  // general case (b) list with >=2 elements
    list->head = remove->next;
    list->head->prev = NULL;
  }

  free(remove);
  remove = NULL;
  list->num_elements -= 1;
  return true;
}

void LinkedList_Append(LinkedList *list, LLPayload_t payload) {
  Verify333(list != NULL);

  LinkedListNode *ln = (LinkedListNode *) malloc(sizeof(LinkedListNode));
  ln->payload = payload;
  ln->next = NULL;
  if (list->num_elements == 0) {
    ln->prev = NULL;
    list->tail = list->head = ln;
  } else {
    ln->prev = list->tail;
    list->tail->next = ln;
    list->tail = ln;
  }
  list->num_elements += 1;
}

void LinkedList_Sort(LinkedList *list, bool ascending,
                     LLPayloadComparatorFnPtr comparator_function) {
  Verify333(list != NULL);
  if (list->num_elements < 2) {
    // No sorting needed.
    return;
  }

  // We'll implement bubblesort! Nnice and easy, and nice and slow :)
  int swapped;
  do {
    LinkedListNode *curnode;

    swapped = 0;
    curnode = list->head;
    while (curnode->next != NULL) {
      int compare_result = comparator_function(curnode->payload,
                                               curnode->next->payload);
      if (ascending) {
        compare_result *= -1;
      }
      if (compare_result < 0) {
        // Bubble-swap the payloads.
        LLPayload_t tmp;
        tmp = curnode->payload;
        curnode->payload = curnode->next->payload;
        curnode->next->payload = tmp;
        swapped = 1;
      }
      curnode = curnode->next;
    }
  } while (swapped);
}


///////////////////////////////////////////////////////////////////////////////
// LLIterator implementation.

LLIterator* LLIterator_Allocate(LinkedList *list) {
  Verify333(list != NULL);

  // OK, let's manufacture an iterator.
  LLIterator *li = (LLIterator *) malloc(sizeof(LLIterator));
  Verify333(li != NULL);

  // Set up the iterator.
  li->list = list;
  li->node = list->head;

  return li;
}

void LLIterator_Free(LLIterator *iter) {
  Verify333(iter != NULL);
  free(iter);
}

bool LLIterator_IsValid(LLIterator *iter) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);

  return (iter->node != NULL);
}

bool LLIterator_Next(LLIterator *iter) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);
  Verify333(iter->node != NULL);

  iter->node = iter->node->next;
  if (iter->node == NULL) {
    return false;
  }

  return true;
}

void LLIterator_Get(LLIterator *iter, LLPayload_t *payload) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);
  Verify333(iter->node != NULL);

  *payload = iter->node->payload;
}

bool LLIterator_Remove(LLIterator *iter,
                       LLPayloadFreeFnPtr payload_free_function) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);
  Verify333(iter->node != NULL);

  LinkedListNode *myNode = iter->node;
  LinkedListNode *myNext = myNode->next;
  LinkedListNode *myPrev = myNode->prev;
  LinkedList *myList = iter->list;
  (*payload_free_function)(myNode->payload);

  if (myList->num_elements == 1) {  // degenerate case (1)
    myList->head = myList->tail = NULL;
    iter->node = NULL;
    free(myNode);
    myNode = NULL;
    myList->num_elements -= 1;
    return false;
  } else if (myList->head == myNode) {  // degenerate case (2)
    myList->head = myNext;
    myNext->prev = NULL;
    iter->node = myNext;
  } else if (myList->tail == myNode) {  // degenerate case (3)
    myList->tail = myPrev;
    myPrev->next = NULL;
    iter->node = myPrev;
  } else {  // fully general case
    myNext->prev = myPrev;
    myPrev->next = myNext;
    iter->node = myNext;
  }

  free(myNode);
  myNode = NULL;
  myList->num_elements -= 1;


  return true;
}


///////////////////////////////////////////////////////////////////////////////
// Helper functions

bool LLSlice(LinkedList *list, LLPayload_t *payload_ptr) {
  Verify333(payload_ptr != NULL);
  Verify333(list != NULL);

  if (list->num_elements == 0) {
    return false;
  }

  LinkedListNode *remove = list->tail;
  *payload_ptr = remove->payload;
  if (list->num_elements == 1) {
    list->tail = list->head = NULL;
  } else {
    list->tail = remove->prev;
    list->tail->next = NULL;
  }

  free(remove);
  remove = NULL;
  list->num_elements -= 1;
  return true;
}

void LLIteratorRewind(LLIterator *iter) {
  iter->node = iter->list->head;
}
