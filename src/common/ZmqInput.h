/** Copyright (C) 2016-2018 European Spallation Source */
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the ZmqInput class for
/// receiving neutron events via ZMQ sockets.
///
//===----------------------------------------------------------------------===//

#pragma once

#include <zmq.h>
#include <common/Socket.h>

class ZMQSubscriber {
public:
  /// \brief Connect to a ZMQ source by ip address/hostname and port
  ZMQSubscriber(Socket::Endpoint &publisher);

  /// \brief Clean up the context and subscriber socket
  virtual ~ZMQSubscriber();
  
  /// Receive data on socket into buffer with specified length
  ssize_t receive(void *receiveBuffer, int bufferSize);

private:
  void* Context{nullptr};
  void* Subscriber{nullptr};
};

class ZMQPublisher {
public:
  /// \brief Connect to a ZMQ source by ip address/hostname and port
  ZMQPublisher(Socket::Endpoint &publisher);

  /// \brief Clean up the context and subscriber socket
  virtual ~ZMQPublisher();
  
  /// Send data on socket from the buffer with specified length
  int send(void *buffer, int bsize);

private:
  void* Context{nullptr};
  void* Publisher{nullptr};
};

