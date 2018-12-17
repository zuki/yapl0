#ifndef TABLE_HPP
#define TABLE_HPP

#include "log.hpp"
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <vector>

/**
  * nameの種類
  */
enum NameType {
  CONST,
  VAR,
  PARAM,
  FUNC
};

struct NameTypeStr : public std::string {
  NameTypeStr(NameType t) {
    switch(t) {
    case CONST : { assign("CONST"); break; }
    case VAR   : { assign("VAR");   break; }
    case PARAM : { assign("PARAM"); break; }
    case FUNC  : { assign("FUNC");  break; }
    }
  }
};

class SymInfo {
public:
  SymInfo(int p_level, NameType p_type, std::string p_name, int p_num): level(p_level),type(p_type),name(p_name),num(p_num){}

public:
  int level;
  NameType type;
  std::string name;
  int num;        // Func: 引数の数
};

class SymTable {
private:
  std::vector<SymInfo> symbolTable;      // 名前シンボルテーブル
  std::vector<std::string> tempNames;    // 一時的な名前テーブル
  int cur_level = -1;

public:
  void blockIn() { cur_level++; }
  void blockOut() {
    while (!symbolTable.empty() && symbolTable.back().level == cur_level) {
      symbolTable.pop_back();
    }
    cur_level--;
  }

  void addSymbol(std::string name, NameType type, int num) {
    symbolTable.emplace_back(cur_level, type, name, num);
  }

  void addSymbol(std::string name, NameType type) {
    addSymbol(name, type, -1);
  }

  bool findSymbol(const std::string &name, const NameType &type, const bool &checkLevel, const int &num) {
    auto result = std::find_if(symbolTable.crbegin(), symbolTable.crend(),
    [&](const SymInfo &e) {
      auto cond = e.name == name && e.type == type;
      if (num != -1) cond = cond && e.num == num;
      if (checkLevel) cond = cond && e.level == cur_level;
      return cond;
    });
    return result == symbolTable.crend() ? false : true;
  }

  bool findSymbol(const std::string &name, const NameType &type) {
    return findSymbol(name, type, true, -1);
  }

  void addTemp(std::string name) {
    tempNames.push_back(name);
  }

  void deleteTemp(const std::string &name) {
    auto it = std::find(tempNames.begin(), tempNames.end(), name);
    if (it != tempNames.end())
      tempNames.erase(it);
  }

  bool findTemp(const std::string &name) {
    auto it = std::find(tempNames.cbegin(), tempNames.cend(), name);
    return it == tempNames.cend() ? false : true;
  }

  bool remainedTemp() {
    return tempNames.size() > 0;
  }

  void dumpSymbolTable() {
    for (const SymInfo &e : symbolTable)
      fprintf(stderr, "[%d] %-10s %s\n", e.level, e.name.c_str(), NameTypeStr(e.type).c_str());
  }

  void dumpTempNames() {
    fprintf(stderr, "remain symbols:");
    for (const std::string &name : tempNames)
      fprintf(stderr, " %s", name.c_str());
    fprintf(stderr, "\n");
  }

};

class CodeInfo {
public:
  CodeInfo(const std::string &name, NameType type, llvm::Function *func,
         llvm::Value *val, int level)
      : name(name), type(type), func(func), val(val), level(level) {}

public:
  std::string name;
  NameType type;
  llvm::Function *func;
  llvm::Value *val;
  int level;
};

class CodeTable {
public:
  const CodeInfo &find(const std::string &name) const {
    auto itr =
        std::find_if(infos.rbegin(), infos.rend(),
                     [&](const CodeInfo &info) { return info.name == name; });
    if (itr == infos.rend()) {
      Log::error((name +" is undefined").c_str(), true);
    }

    return *itr;
  }

  void appendConst(const std::string &name, llvm::Value *val) {
    infos.emplace_back(name, CONST, nullptr, val, cur_level);
  }

  void appendVar(const std::string &name, llvm::Value *val) {
    infos.emplace_back(name, VAR, nullptr, val, cur_level);
  }

  void appendParam(const std::string &name, llvm::Value *val) {
    infos.emplace_back(name, PARAM, nullptr, val, cur_level);
  }

  void appendFunction(const std::string &name, llvm::Function *func) {
    infos.emplace_back(name, FUNC, func, nullptr, cur_level);
  }

  void enterBlock() { cur_level++; }

  void leaveBlock() {
    while (!infos.empty() && infos.back().level == cur_level) {
      infos.pop_back();
    }
    cur_level--;
  }

  int getLevel() const { return cur_level; }

  void dumpInfos() {
    for (const CodeInfo &info : infos)
      fprintf(stderr, "[%d] %-10s %s\n", info.level, info.name.c_str(), NameTypeStr(info.type).c_str());
  }

private:
  std::vector<CodeInfo> infos;
  int cur_level = -1;
};

#endif
