#pragma once

#include "error.hpp"
#include <cassert>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <vector>

enum class IdType {
  CONST,
  VAR,
  PARAM,
  FUNC,
};

class IdInfo {
public:
  IdInfo(const std::string &name, IdType type, llvm::Function *func,
         llvm::Value *val, size_t level)
      : name(name), type(type), func(func), val(val), level(level) {}

public:
  std::string name;
  IdType type;
  llvm::Function *func;
  llvm::Value *val;
  size_t level;
};

class Table {
public:
  const IdInfo &find(const std::string &name) const {
    auto itr =
        std::find_if(infos.rbegin(), infos.rend(),
                     [&](const IdInfo &info) { return info.name == name; });
    if (itr == infos.rend()) {
      undefinedError(name);
    }

    return *itr;
  }

  void appendConst(const std::string &name, llvm::Value *val) {
    infos.emplace_back(name, IdType::CONST, nullptr, val, cur_level);
  }

  void appendVar(const std::string &name, llvm::Value *val) {
    infos.emplace_back(name, IdType::VAR, nullptr, val, cur_level);
  }

  void appendParam(const std::string &name) {
    infos.emplace_back(name, IdType::PARAM, nullptr, nullptr, cur_level);
  }

  void appendFunction(const std::string &name, llvm::Function *func) {
    infos.emplace_back(name, IdType::FUNC, func, nullptr, cur_level);
  }

  void enterBlock() { cur_level++; }
  void leaveBlock() {
    while (!infos.empty() && infos.back().level == cur_level) {
      infos.pop_back();
    }
    cur_level--;
  }

  size_t getLevel() const { return cur_level; }

private:
  std::vector<IdInfo> infos;
  size_t cur_level;
};
