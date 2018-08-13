#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "uv.h"
#include "http_parser/http_parser.h"

#define SERVER_PORT 8000
#define SERVER_RESPONSE \
  "HTTP/1.1 200 OK\r\n" \
  "Content-Type: text/plain\r\n" \
  "Content-Length: 12\r\n" \
  "\r\n" \
  "Hello World\n"

static http_parser_settings settings;

static uv_buf_t resBuf;

typedef struct {
  uv_tcp_t handle;
  http_parser parser;
} client_t;

void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  if (buf->base != NULL) {
    buf->len = suggested_size;
  }
}

static void on_close(uv_handle_t* handle) {
  free(handle);
}

void after_shutdown(uv_shutdown_t* req, int status) {
  if (status < 0)
    fprintf(stderr, "after_shutdown err: %s\n", uv_strerror(status));

  uv_close((uv_handle_t*) req->handle, on_close);
  free(req);

  printf("Connection closed\n");
}

void after_write(uv_write_t* req, int status) {
  // printf("afterWrite() called\n");

  if (status == 0)
    return;


  if (status == UV_ECANCELED)
    return;

  uv_close((uv_handle_t*)req->handle, on_close);
}

void on_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
  int r;
  uv_shutdown_t* req;
  client_t* client = handle->data;
  size_t parsed;

  // printf("on_read: %i\n", handle);

  if (nread >= 0) {
    // New line of data received
    // printf("\nNew line of data: %zi\n", nread);
    // printf("Data received: %s\n", buf->base);
  
    parsed = http_parser_execute(&client->parser, &settings, buf->base, nread);

    if (parsed < nread) {
      fprintf(stderr, "Unable to parse chunk with http_parser\n");
      uv_close((uv_handle_t*) handle, on_close);
    }
  }

  if (nread <= 0 && buf->base != NULL)
    free(buf->base);

  if (nread == 0)
    return;

  if (nread < 0) {
    // Encounter end of file (UV_EOF)
    fprintf(stderr, "after_read err: %s\n", uv_strerror(nread));

    req = (uv_shutdown_t*) malloc(sizeof(*req));
    assert(req != NULL);

    r = uv_shutdown(req, handle, after_shutdown);
    
    if (r) {
      fprintf(stderr, "uv_shutdown error: %s\n", uv_strerror(r));
    }

    return;
  }
}

void on_new_connection(uv_stream_t *server, int status) {
  printf("New connection established\n");

  if (status != 0) {
    fprintf(stderr, "on_new_connection error: %s\n", uv_strerror(status));
    return;
  }

  int r;

  client_t* client = malloc(sizeof(client_t));

  // printf("on_connection: %i\n", client);

  r = uv_tcp_init(uv_default_loop(), &client->handle);

  if (r) {
    fprintf(stderr, "uv_tcp_init error: %s\n", uv_strerror(r));
    return;
  }

  client->handle.data = client;

  r= uv_accept(server, (uv_stream_t*) &client->handle);

  if (r) {
    fprintf(stderr, "uv_accept error: %s\n", uv_strerror(r));
    return;
  }

  // Init http parser
  http_parser_init(&client->parser, HTTP_REQUEST);
  client->parser.data = client;

  r = uv_read_start((uv_stream_t*) &client->handle, alloc_cb, on_read);

  if (r) {
    fprintf(stderr, "uv_read_start error: %s\n", uv_strerror(r));
    return;
  }
}

void init_tcp_server(uv_loop_t* loop) {
  uv_tcp_t* tcp_server_ptr;
  struct sockaddr_in bind_addr;
  int r;

  // Define ip v4 address to bind
  uv_ip4_addr("0.0.0.0", SERVER_PORT, &bind_addr);
  
  // Init server and bind to defined address
  tcp_server_ptr = (uv_tcp_t*) malloc(sizeof(*tcp_server_ptr));
  uv_tcp_init(loop, tcp_server_ptr);
  
  r = uv_tcp_bind(tcp_server_ptr,
    (const struct sockaddr *)&bind_addr,
    0);
  if (r) {
    fprintf(stderr, "uv_tcp_bind error: %s\n", uv_strerror(r));
    return;
  }
  
  r = uv_listen((uv_stream_t *) tcp_server_ptr, SOMAXCONN, on_new_connection);
  if (r) {
    fprintf(stderr, "uv_listen error: %s\n", uv_strerror(r));
    return;
  }
  
  printf("Server listening on port %i\n", SERVER_PORT);
}

int on_headers_complete(http_parser* parser) {
  uv_write_t* writeReq;
  client_t* client = parser->data;

  // Error handling
  printf("http headers complete!\n");

  // Sending response
  writeReq = (uv_write_t*) malloc(sizeof(*writeReq));

  uv_write(writeReq, (uv_stream_t*) &client->handle, &resBuf, 1, after_write);

  return 1;
}

int main(int argc, char const *argv[]) {
  uv_loop_t* default_loop_ptr;

  resBuf.base = SERVER_RESPONSE;
  resBuf.len = sizeof(SERVER_RESPONSE);

  settings.on_headers_complete = on_headers_complete;

  default_loop_ptr = uv_default_loop();
  init_tcp_server(default_loop_ptr);

  uv_run(default_loop_ptr, UV_RUN_DEFAULT);

  return 0;
}
