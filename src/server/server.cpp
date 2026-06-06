#include "server/server.h"
#include "server/connection.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

// Main event loop: wait -> accept -> read -> parse -> handle -> queue write

Server::Server(uint16_t port, IStorageEngine &storage)
    : port_(port), storage_(storage) {}

void Server::run() {
  // creates a TCP socket and returns a file descriptor listen_fd (identifier)
  // SOCK_NONBLOCK means read/write never block, return whatever data available
  int listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

  // SO_REUSEADDR lets you rebind to the same port after the server restarts,
  // instead of waiting for OS to release it
  int opt = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;         // IPv4
  addr.sin_port = htons(port_);      // port number
  addr.sin_addr.s_addr = INADDR_ANY; // accept connections on any interface

  // bind() will assign the address and port to the socket.
  // convert the port to big endian network byte order for different CPU types
  // INADDR_ANY to listn on all network interfaces
  bind(listen_fd, (sockaddr *)&addr, sizeof(addr));

  // will mark the socket as passive so it will accept incoming connections. 128
  // max pending connections the kernel will queue
  listen(listen_fd, 128);

  // RESULT: IS now has a dor at 0.0.0.0:port_. when the client connects the
  // kernel queues the connections at listen_fd

  // asks the kernel for an epoll instance. return epoll_fd (file descriptor)
  // representing the thing that watches other file descriptors (network socket,
  // pipes, eventfd), to see if they are ready for IO
  int epoll_fd = epoll_create1(0);

  // EPOLLIN - wake up when the fd has data to read. register interest in events
  // on specific fds.
  epoll_event ev{EPOLLIN, {.fd = listen_fd}};

  // check kernel , to watch listen_fd for incoming data (so new connections)
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

  // maps client socket fd, per-client state (parser + write buffer). each
  // connection gets its own RESPReader so partial reads dont mix between
  // clients
  std::unordered_map<int, Connection> connections;

  // the buffer where the epoll write the list of ready fds. starts at 64 but
  // can be resized if needed (when opell tells you how many reads are ready)
  std::vector<epoll_event> events(64);

  while (true) {
    // blocks the thread until at least one fd has an event
    // returns n number of ready fds and writes them into events[]
    // -1 being no timeout, wait forever
    // RESULT : instead of checking n sockets in a loop of busy waiting we can
    // make one syscall and the kernel tells use which 3 sockets are ready
    int n = epoll_wait(epoll_fd, events.data(), events.size(), -1);

    // for each fd find out what to do
    for (int i = 0; i < n; i++) {
      int fd = events[i].data.fd;

      // the accept4 creates a new socket (conn fd) for this specific clinet.
      // the SOCK_NONBLCOK wil make it nonblocking for epll so you never block
      // on one client
      // inner while true accepts all pending connections at once. so it can
      // accept multiple between epoll waits, accept4 returns -1 with errno =
      // EAGAIN when non are left.
      // Register conn_fd for both EPOLLIN (data from client) and EPOLLOUT
      // (socket ready to send)
      // creates a Connection object to hold per-client state
      if (fd == listen_fd) {
        while (true) {
          int conn_fd = accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK);
          if (conn_fd == -1)
            break;
          epoll_event conn_ev{EPOLLIN | EPOLLOUT, {.fd = conn_fd}};
          epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &conn_ev);
          connections.emplace(conn_fd, Connection{});
        }
        continue;
      }

      // looks up the per client state
      auto it = connections.find(fd);
      if (it == connections.end())
        continue;

      bool closed = false;

      // read for non blocking , if nread <=0 if client disconnected 0 EOF, -1
      // error, we close the socket and remove the connection
      // if nread > 0 passes the data to connection::on_readable() (more
      // there..)
      if (events[i].events & EPOLLIN) {
        char buf[65536];
        ssize_t nread = read(fd, buf, sizeof(buf));
        if (nread <= 0) {
          close(fd);
          connections.erase(it);
          closed = true;
        } else {
          it->second.on_readable(buf, nread, storage_);
        }
      }

      // only if the socket has space to write (client is consuming data) more
      // at : Connection::on_writable
      if (!closed && (events[i].events & EPOLLOUT)) {
        it->second.on_writable(fd);
      }
    }
  }
}
