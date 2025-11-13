#include "clambda.h"
#include "json.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Simple hello example.
 * Returns "Hello there <input>!".
 * If input starts with "bad" then returns error.
 */
bool hello(Invocation *inv) {
  if(strncmp(inv->payload, "bad", 3) == 0) {
    return false;
  }
  inv->response_type = TEXT;
  fprintf(inv->out, "Hello there %.*s!", (int) inv->size, inv->payload);
  return true;
}


/* Counter handler, has _init function and maintains state
 * between calls.
 */

typedef struct Counter {
  long count;
} Counter;

void *counter_init() {
  printf("Counter init called!\n");
  Counter *c = malloc(sizeof(Counter));
  if(!c) {
    fprintf(stderr, "Failed to allocate counter\n");
    return NULL;
  }
  c->count = 0;
  return c;
}

bool counter(Invocation *inv) {
  if(!inv->user_data) return false;

  Counter *c = (Counter*) inv->user_data;
  printf("counter is %p\n", c);
  c->count += 1;
  inv->response_type = JSON;
  fprintf(inv->out, "{\"count\": %ld}", c->count);
  return true;
}


/* Line analyzer, reads JSON line and outputs some info about it */

typedef struct Point {
  double x, y;
} Point;

typedef struct Line {
  char *id;
  size_t npoints;
  Point *points;
} Line;

bool parse_point(char **at, Point *p) {
  json_object(at, {
    json_field("x", json_double, &p->x);
    json_field("y", json_double, &p->y);
  });
  return true;
}

bool parse_points(char **at, Line *line) {
  json_array(at, &line->points, &line->npoints, Point, parse_point);
  return true;
}

bool parse_line(char **at, Line *line) {
  json_object(at, {
    json_field("id", json_string_ptr, &line->id);
    json_field("points", parse_points, line);
  });
  return true;
}

bool analyze_line(Invocation *inv) {
  char *json = (char*) inv->payload;
  char **at = &json;
  Line line = {0};
  if(!parse_line(at, &line)) return false;

  printf("Line %s has %d points\n", line.id, line.npoints);
  double len = 0;
  if(line.npoints) {
    for(int i=0; i<line.npoints-1; i++) {
      double dx = abs(line.points[i].x - line.points[i+1].x);
      double dy = abs(line.points[i].y - line.points[i+1].y);
      len += sqrt(dx*dx + dy*dy);
    }
  }
  fprintf(inv->out, "{\"npoints\": %d, \"length\": %f}",
          line.npoints, len);

  // release
  free(line.points);
  return true;
}
