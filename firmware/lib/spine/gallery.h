#pragma once
#include <map>
#include <string>
#include <vector>

namespace spine {

class Gallery {
 public:
  struct Entry {
    std::string pet;
    std::vector<float> embedding;
  };
  struct Match {
    std::string pet;
    float sim = -1.0f;
  };

  void add(const std::string& path, const std::string& pet, std::vector<float> embedding);
  void prune(const std::vector<std::string>& paths);

  Match match(const std::vector<float>& embedding) const;

  size_t size() const { return entries_.size(); }

  std::string toJson() const;
  bool fromJson(const std::string& blob);

 private:
  std::map<std::string, Entry> entries_;
};

}
