/** Copyright (C) 2016-2018 European Spallation Source */
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of ZmqInput
///
//===----------------------------------------------------------------------===//
//
#include <common/Log.h>
#include <common/Socket.h>
#include <common/ZmqInput.h>
#include <fmt/format.h>

ZMQSubscriber::ZMQSubscriber(Socket::Endpoint &source) {
  Context = zmq_ctx_new();
  Subscriber = zmq_socket(Context, ZMQ_SUB);
  zmq_setsockopt(Subscriber, ZMQ_SUBSCRIBE, "", 0);
  auto addr = fmt::format("tcp://{}:{}", source.IpAddress, source.Port);
  int rc = zmq_connect(Subscriber, addr.c_str());
  if (rc != 0) {
    LOG(IPC, Sev::Error, "ZMQ subscriber error : %d : %s\n", errno, zmq_strerror (errno));
    throw std::runtime_error("failed to connect zmq subscriber socket");
  }
}

ZMQSubscriber::~ZMQSubscriber() {
  if (Subscriber != nullptr)
    zmq_close(Subscriber);
  if (Context != nullptr) {
    zmq_ctx_destroy(Context);
  }
}

ssize_t ZMQSubscriber::receive(void *receiveBuffer, int bufferSize) {
  return zmq_recv(Subscriber, receiveBuffer, bufferSize, 0);
}

ZMQPublisher::ZMQPublisher(Socket::Endpoint &source) {
  Context = zmq_ctx_new();
  Publisher = zmq_socket(Context, ZMQ_PUB);
  auto addr = fmt::format("tcp://*:{}", source.Port);
  auto rc = zmq_bind(Publisher, addr.c_str());
  if (rc != 0) {
    LOG(IPC, Sev::Error, "ZMQ publisher error : %d : %s\n", errno, zmq_strerror (errno));
    throw std::runtime_error("failed to bind zmq publish socket");
  }
}

ZMQPublisher::~ZMQPublisher() {
  if (Publisher != nullptr)
    zmq_close(Publisher);
  if (Context != nullptr) {
    zmq_ctx_destroy(Context);
  }
}

int ZMQPublisher::send(void *buffer, int bufferSize) {
  return zmq_send(Publisher, buffer, bufferSize, 0);
}