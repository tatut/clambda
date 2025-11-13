#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dlfcn.h>

#include <curl/curl.h>
#include "clambda.h"

char runtime_api_url[512];
CURL *curl;

char *find_sep(char *start, size_t len, const char *sep) {
  char *end = start+len;
  while (start < end-1) {
    if (*start == sep[0] && *(start + 1) == sep[1]) {
      return start;
    }
    start++;
  }
  return NULL;
}

size_t read_invocation(void *buffer, size_t size, size_t nmemb,
                       Invocation *inv) {
  inv->size = size * nmemb;
  if(inv->size) {
    inv->payload = realloc(inv->payload, inv->size + 1);
    if (!inv->payload) {
      fprintf(stderr, "Unable to realloc payload to %zu\n", inv->size);
      inv->success = false;
    } else {
      memcpy(inv->payload, buffer, inv->size);
      inv->payload[inv->size] = 0; // NUL terminator
    }
  }
  return inv->size;
}

size_t read_invocation_id(char *buffer, size_t size, size_t nitems,
                          Invocation *inv) {
  size_t len = size * nitems;
  char *sep = find_sep(buffer, len, ": ");
  while (sep) {
    char *name = buffer;
    len = len - (sep - buffer + 2);
    //printf("header name: %.*s\n", (int)(sep - buffer), name);
    buffer = sep+2;
    char *end = find_sep(buffer, len, "\r\n");
    if (end) {
      //printf("header val: %.*s\n", (int)(end-buffer), buffer);
      if (strncmp(name, "Lambda-Runtime-Aws-Request-Id", 29) == 0) {
        printf("GOT ID: %.*s\n", (int)(end-buffer), buffer);
        snprintf(inv->request_id, 128, "%.*s", (int) (end-buffer), buffer);
        goto done;
      }
      len = len - (end - buffer + 2);
      buffer = end + 2;
      sep = find_sep(buffer, len, ": ");
    } else {
      goto done;
    }
  }
done:
  return size*nitems;
}

void next_invocation(Invocation *inv) {
  char url[512];
  inv->request_id[0] = 0;
  inv->success = true;
  snprintf(url, 512, "%s/next", runtime_api_url);
  printf("get next invocation: %s\n", url);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, read_invocation_id);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, inv);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, read_invocation);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, inv);
  int res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "Failed to read next invocation!");
    inv->success = false;
  }
  if (inv->request_id[0] == 0) {
    inv->success = false;
  }
  curl_easy_reset(curl);
  printf(" invocation done, res=%d!\n", res);
}

void send_response(Invocation *inv, char *payload, size_t size) {
  printf("sending response for %s\n", inv->request_id);
  char url[512];
  snprintf(url, 512, "%s/%s/response", runtime_api_url, inv->request_id);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  struct curl_slist *list = NULL;
  switch (inv->response_type) {
  case TEXT:   curl_slist_append(list, "Content-Type: text/plain"); break;
  case JSON:   curl_slist_append(list, "Content-Type: application/json"); break;
  case BINARY: curl_slist_append(list, "Content-Type: application/octet-stream"); break;
  }
  printf("response \"%.*s\"\n", (int)size, payload);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, size);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

  curl_easy_perform(curl);
  curl_slist_free_all(list);
  curl_easy_reset(curl);
  // FIXME: handle failure!
}

const char *error_json = "{\"errorMessage\": \"Error in clambda handler\""
                         ",\"errorType\": \"HandlerReturnedError\"}";


void send_error(Invocation *inv) {
  char url[512];
  snprintf(url, 512, "%s/%s/error", runtime_api_url, inv->request_id);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  struct curl_slist *list = NULL;
  curl_slist_append(list, "Content-Type: application/json");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, error_json);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(error_json));
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
  curl_easy_perform(curl);
  curl_slist_free_all(list);
  curl_easy_reset(curl);
  // FIXME: handle failure in sending error?
}

Handler load_handler() {
  char *handler = getenv("_HANDLER");
  if (!handler) {
    fprintf(stderr, "No _HANDLER environment variable.\n");
    exit(1);
  }
  char *fn = strchr(handler, '.');
  if (!fn) {
    fprintf(stderr, "Expected _HANDLER <module>.<handler fn>, got: %s\n",
            handler);
    exit(1);
  }

  *fn = 0;
  fn++;
  printf("use handler, module=%s, fn=%s\n", handler, fn);
  char path[512];
  snprintf(path, 512, "%s.o", handler);
  void *handler_lib = dlopen(path, RTLD_LAZY);
  if (!handler_lib) {
    fprintf(stderr, "Unable to load handler library: %s\n", path);
    exit(2);
  }
  char func[64];
  snprintf(func, 64, "%s_init", fn);
  void *(*init)() = dlsym(handler_lib, func);

  Handler h = {0};
  if (init) {
    h.user_data = init();
  }
  h.handle = dlsym(handler_lib, fn);
  if (!h.handle) {
    fprintf(stderr, "Unable to find handler function named: %s\n", fn);
    exit(2);
  }
  return h;
}

int main(int argc, char **argv) {
  const char *aws_lambda_runtime_api = getenv("AWS_LAMBDA_RUNTIME_API");
  snprintf(runtime_api_url, 512, "http://%s/2018-06-01/runtime/invocation",
           aws_lambda_runtime_api);

  printf("Using AWS Lambda URL: %s\n", runtime_api_url);
  Handler h = load_handler();
  curl = curl_easy_init();
  Invocation inv = (Invocation) {.payload = NULL};
  for (;;) {
    next_invocation(&inv);
    if (inv.success) {
      char *out_buf = NULL;
      size_t out_size = 0;
      inv.out = open_memstream(&out_buf, &out_size);
      inv.user_data = h.user_data;
      inv.response_type = JSON;
      if (h.handle(&inv)) {
        fflush(inv.out);
        send_response(&inv, out_buf, out_size);
      } else {
        send_error(&inv);
      }
      fclose(inv.out);
      if(out_buf) free(out_buf);
    }
  }

}
