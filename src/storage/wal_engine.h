#pragma once
#include "storage/storage.h"
#include <string>

class WalEngine : public IStorageEngine {
public:
  WalEngine(IStorageEngine &inner, const std::string &path);
  ~WalEngine();

  std::optional<std::string> get(const std::string &key) override;
  void set(const std::string &key, const std::string &value) override;
  bool del(const std::string &key) override;
  bool exists(const std::string &key) override;

  void replay();

private:
  void append(const std::string &data);
  IStorageEngine &inner_;
  int fd_; // for the wal.log
  std::string path_;
};
