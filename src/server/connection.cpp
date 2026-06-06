#include "server/connection.h"
#include "commands/handler.h"
#include <unistd.h>

void Connection::on_readable(const char *data, size_t len,
                             IStorageEngine &storage) {
  parser.feed(data, len);               // add raw bytes to parser buffer
  while (auto cmd = parser.try_parse()) // parse as many commands as possible
    write_buf += RESPReader::serialize(handle_command(*cmd, storage));
  // queue replies a single tcp read might contain multiple commands
  // (e.g pipelining) it parses and handles all of them appending each response
  // to write_buf
}

// write() sends queued bytes. it may send partial data (returns fewer bytes
// than requested). non-blocking.
// the remaining bytes stay in write_buf for the next EPOLLOUT event
// the buffer will grouw if client can keep up but the server keeps handling
// other clients
void Connection::on_writable(int fd) {
  if (write_buf.empty())
    return;
  ssize_t n = write(fd, write_buf.data(), write_buf.size());
  if (n > 0)
    write_buf.erase(0, n);
}
