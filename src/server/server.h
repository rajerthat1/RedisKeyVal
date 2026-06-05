#pragma once
#include "storage/storage.h"
#include <cstdint>

class Server {
public:
  Server(uint16_t port, IStorageEngine &storage);
  void run();

private:
  uint16_t port_;
  IStorageEngine &storage_;
};
