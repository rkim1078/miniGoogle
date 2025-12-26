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

#include <stdio.h>       // for snprintf()
#include <unistd.h>      // for close(), fcntl()
#include <sys/types.h>   // for socket(), getaddrinfo(), etc.
#include <sys/socket.h>  // for socket(), getaddrinfo(), etc.
#include <arpa/inet.h>   // for inet_ntop()
#include <netdb.h>       // for getaddrinfo()
#include <errno.h>       // for errno, used by strerror()
#include <string.h>      // for memset, strerror()
#include <iostream>      // for std::cerr, etc.

#include "./ServerSocket.h"

extern "C" {
  #include "libhw1/CSE333.h"
}

using std::string;

namespace hw4 {

ServerSocket::ServerSocket(uint16_t port) {
  port_ = port;
  listen_sock_fd_ = -1;
}

ServerSocket::~ServerSocket() {
  // Close the listening socket if it's not zero.  The rest of this
  // class will make sure to zero out the socket if it is closed
  // elsewhere.
  if (listen_sock_fd_ != -1)
    close(listen_sock_fd_);
  listen_sock_fd_ = -1;
}

bool ServerSocket::BindAndListen(int ai_family, int *const listen_fd) {
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = ai_family;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_flags |= AI_V4MAPPED;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_canonname = nullptr;
  hints.ai_addr = nullptr;
  hints.ai_next = nullptr;

  // store port_ as a string for use in getaddrinfo
  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%hu", port_);

  // get a list of address structures in result
  struct addrinfo *result;
  int res = getaddrinfo(nullptr, port_str, &hints, &result);
  if (res != 0) {
    std::cerr << "getaddrinfo failed: " << gai_strerror(res) << std::endl;
    return false;
  }

  // of these addr structs, we try to create a socket and bind to one
  int sockfd = -1;
  for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
    sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sockfd == -1) {
      continue;  // try next
    }

    // once we've created a socket, configure it efficiently
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // try binding to it and making this a listening socket
    if (bind(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
      if (listen(sockfd, SOMAXCONN) == 0) {
        // success, exit
        *listen_fd = sockfd;  // update output param
        listen_sock_fd_ = sockfd;
        sock_family_ = rp->ai_family;
        freeaddrinfo(result);
        return true;
      }
    }

    // bind failed, try again
    close(sockfd);
    sockfd = -1;
  }
  // cleanup
  freeaddrinfo(result);
  return false;
}

bool ServerSocket::Accept(int *const accepted_fd,
                          string *const client_addr,
                          uint16_t *const client_port,
                          string *const client_dns_name,
                          string *const server_addr,
                          string *const server_dns_name) const {

  // accept a connection from a client and store client addr info in caddr
  struct sockaddr_storage caddr;
  socklen_t caddr_len = sizeof(caddr);
  int client_fd = accept(listen_sock_fd_,
                   reinterpret_cast<struct sockaddr*>(&caddr),
                   &caddr_len);
  // return false on failure
  if (client_fd < 0) {
    return false;
  }

  // fill out client side info return params
  // use getnameinfo to convert client addr into IP and port
  char host[NI_MAXHOST], serv[NI_MAXSERV];
  if (getnameinfo(reinterpret_cast<struct sockaddr*>(&caddr), caddr_len,
                  host, sizeof(host), serv, sizeof(serv),
                  NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
    *client_addr = host;
    *client_port = static_cast<uint16_t>(atoi(serv));
  } else {  // insurance, dont need to check for getnameinfo failure for hw4
    *client_addr = "unknown";
    *client_port = 0;
  }

  // reverse DNS lookup for client hostname
  char dnsbuf[NI_MAXHOST];
  if (getnameinfo(reinterpret_cast<struct sockaddr*>(&caddr), caddr_len,
                  dnsbuf, sizeof(dnsbuf), nullptr, 0, 0) == 0) {
    *client_dns_name = dnsbuf;
  } else {  // insurance, dont need to check for getnameinfo failure for hw4
    *client_dns_name = *client_addr;
  }

  // reverse DNS not stable across environments, override default Docker/macOS clientside addr to localhost
  if (*client_addr == "127.0.0.1" || *client_addr == "::1" || *client_addr == "::ffff:127.0.0.1") {
    *client_dns_name = "localhost";
  }

  // fill out server side info return params
  // use getsockname to find addr/port that the client's connection is headed
  struct sockaddr_storage saddr;
  socklen_t saddr_len = sizeof(saddr);
  if (getsockname(client_fd, reinterpret_cast<struct sockaddr*>(&saddr),
      &saddr_len) == 0) {
    if (getnameinfo(reinterpret_cast<struct sockaddr*>(&saddr), saddr_len,
                    host, sizeof(host), serv, sizeof(serv),
                    NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
      *server_addr = host;  // server IP
    } else {
      *server_addr = "unknown";
    }

    if (getnameinfo(reinterpret_cast<struct sockaddr*>(&saddr), saddr_len,
                    dnsbuf, sizeof(dnsbuf), nullptr, 0, 0) == 0) {
      *server_dns_name = dnsbuf;
    } else {  // insurance, dont need to check for getnameinfo failure for hw4
      *server_dns_name = *server_addr;
    }
  }

  // reverse DNS not stable across environments, override default Docker/macOS serverside addr to localhost
  if (*server_addr == "127.0.0.1" || *server_addr == "::1" || *server_addr == "::ffff:127.0.0.1") {
    *server_dns_name = "localhost";
  }

  // return new client fd to caller
  *accepted_fd = client_fd;
  return true;
}

}  // namespace hw4
