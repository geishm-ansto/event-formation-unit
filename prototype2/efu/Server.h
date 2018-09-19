/* Copyright (C) 2016-2018 European Spallation Source, ERIC. See LICENSE file */
//===----------------------------------------------------------------------===//
///
/// \file
///
/// Implements a command server. The server supports multiple
/// clients, and is of type request-response. A client request is
/// handled synchronously to completion before servring next request.
///
//===----------------------------------------------------------------------===//

#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <common/EFUArgs.h>
#include <sys/select.h>
#include <sys/types.h>

#define UNUSED __attribute__((unused))

/// \brief Use MSG_SIGNAL on Linuxes
#ifdef MSG_NOSIGNAL
#define SEND_FLAGS MSG_NOSIGNAL
#else
#define SEND_FLAGS 0
#endif

/** \todo make this work with public static unsigned int */
#define SERVER_BUFFER_SIZE 9000U
#define SERVER_MAX_CLIENTS 16
#define SERVER_MAX_BACKLOG 3

static_assert(SERVER_MAX_CLIENTS <= FD_SETSIZE, "Too many clients");

class Server {
public:
  /// \brief Server for program control and stats
  /// \param port tcp port
  /// \param args - needed to access Stat.h counters
  Server(int port, Parser &parse);

  /// \brief Setup socket parameters
  void serverOpen();

  /// \brief Teardown socket
  /// \param socketfd socket file descriptor
  void serverClose(int socketfd);

  /// \brief Called in main program loop
  void serverPoll();

  /// \brief Send reply to Client
  /// \param socketfd socket file descriptor
  int serverSend(int socketfd);

private:
  typedef struct {
    uint8_t buffer[SERVER_BUFFER_SIZE + 1];
    uint32_t bytes;
  } socket_buffer_t;

  socket_buffer_t input;
  socket_buffer_t output;

  int port_{0};
  int serverfd{-1};
  std::array<int, SERVER_MAX_CLIENTS> clientfd;

  struct timeval timeout;

  Parser &parser;
};
