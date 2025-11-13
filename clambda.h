#ifndef clambda_h_
#define clambda_h_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

typedef enum ContentType {
  TEXT,  // text/plain
  JSON,  // application/json
  BINARY // application/octet-stream
} ContentType;


typedef struct Invocation {
  bool success;
  char request_id[128];

  uint8_t *payload;
  size_t size;

  ContentType response_type;
  // File handle to write the response to
  FILE *out;

  void *user_data;
} Invocation;


typedef struct Handler {
  void *user_data;
  bool (*handle)(Invocation*);
} Handler;

#endif
