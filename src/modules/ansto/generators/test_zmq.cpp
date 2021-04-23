#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <zmq.h>

int server(int loop) {
  //  Socket to talk to clients
  void *context = zmq_ctx_new();
  void *responder = zmq_socket(context, ZMQ_REP);
  int rc = zmq_bind(responder, "tcp://*:5555");
  if (rc != 0) {
    printf("server bind failed");
    return rc;
  }

  while (loop-- > 0) {
    char buffer[10];
    zmq_recv(responder, buffer, 10, 0);
    printf("Received Hello\n");
    sleep(1); //  Do some 'work'
    zmq_send(responder, "World", 5, 0);
  }
  return 0;
}

int client(int loop) {
  printf("Connecting to hello world server…\n");
  void *context = zmq_ctx_new();
  void *requester = zmq_socket(context, ZMQ_REQ);
  zmq_connect(requester, "tcp://localhost:5555");

  int request_nbr;
  for (request_nbr = 0; request_nbr < loop; request_nbr++) {
    char buffer[10];
    printf("Sending Hello %d…\n", request_nbr);
    zmq_send(requester, "Hello", 5, 0);
    zmq_recv(requester, buffer, 10, 0);
    printf("Received World %d\n", request_nbr);
  }
  zmq_close(requester);
  zmq_ctx_destroy(context);
  return 0;
}

int publisher(int loop) {
  //  Socket to talk to clients
  void *context = zmq_ctx_new();
  void *publisher = zmq_socket(context, ZMQ_PUB);
  int rc = zmq_bind(publisher, "tcp://*:5555");
  if (rc != 0) {
    printf("publisher bind failed");
    return rc;
  }

  while (loop-- > 0) {
    auto res = zmq_send(publisher, "Hello World", 11, 0);
    if (res == -1) {
      printf("error: %d : %s\n", errno, zmq_strerror(errno));
    }
    sleep(1);
  }
  return 0;
}

int subscriber(int loop) {
  printf("Connecting to hello world server…\n");
  void *context = zmq_ctx_new();
  void *subscriber = zmq_socket(context, ZMQ_SUB);
  int rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
  if (rc == -1) {
    printf("error: %d : %s\n", errno, zmq_strerror(errno));
  }
  rc = zmq_connect(subscriber, "tcp://localhost:5555");
  if (rc != 0) {
    printf("subscriber connect failed");
    return rc;
  }

  int request_nbr;
  for (request_nbr = 0; request_nbr < loop; request_nbr++) {
    char buffer[15];
    zmq_recv(subscriber, buffer, 15, 0);
    printf("Received: %s\n", buffer);
  }
  zmq_close(subscriber);
  zmq_ctx_destroy(context);
  return 0;
}

int main(void) {
  std::thread server_thread(publisher, 5);
  sleep(1);
  std::thread client_thread(subscriber, 3);

  client_thread.join();
  server_thread.join();

  return 0;
}