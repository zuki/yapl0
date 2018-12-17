#ifndef TABLE_HPP
#define TABLE_HPP

#include "log.hpp"
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>

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
  void blockOut();

  void addSymbol(std::string name, NameType type, int num = -1) {
    symbolTable.emplace_back(cur_level, type, name, num);
  }

  bool findSymbol(const std::string &name, const NameType &type, const bool &checkLevel = true, int num = -1) const;

  void addTemp(std::string name) {
    tempNames.push_back(name);
  }

  void deleteTemp(const std::string &name);

  bool findTemp(const std::string &name) const;

  bool remainedTemp() const { return tempNames.size() > 0; }

  void dumpSymbolTable() const;

  void dumpTempNames() const;
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
  const CodeInfo &find(const std::string &name) const;

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

  void leaveBlock();

  int getLevel() const { return cur_level; }

  void dumpInfos() const;

private:
  std::vector<CodeInfo> infos;
  int cur_level = -1;
};

#endif
