#ifndef AST_HPP
#define AST_HPP

#include "llvm/ADT/STLExtras.h"
#include <string>
#include <map>
#include <vector>

/************************************************
AST
************************************************/

/**
  * クラス宣言
  */
class BaseAST;
class ProgramAST;
class BlockAST;
class ConstDeclAST;
class VarDeclAST;
/*
class FuncDeclAST;
class StatementAST;
class AssignAST;
class BeginAST;
class IfThenAST;
class WhileDoAST;
class ReturnAST;
class WriteAST;
class WritelnAST;
class ConditionAST;
class BinaryExprAST;
class NullExprAST;
class VariableAST;
class NumberAST;
class CallExprAST;
*/
/**
  * ASTの種類
  */
enum AstID {
  BaseID,
  ProgramID,
  BlockID,
  ConstDeclID,
  VarDeclID,
  FuncDeclID,
  StatementID,
  AssignID,
  BeginID,
  IfThenID,
  WhileDoID,
  ReturnID,
  WriteID,
  WritelnID,
  ConditionID,
  BinaryExprID,
  NullExprID,
  VariableID,
  NumberID,
  CallExprID
};


/**
  * ASTの基底クラス
  */
class BaseAST {
  AstID ID;

  public:
  BaseAST(AstID id):ID(id){}
  virtual ~BaseAST(){}
  AstID getValueID() const { return ID; }
};


/**
  * ソースコードを表すAST
  */
class ProgramAST {
  std::unique_ptr<BlockAST> Block;

  public:
    ProgramAST(std::unique_ptr<BlockAST> block): Block(std::move(block)) {}
    ~ProgramAST() {}
    std::unique_ptr<BlockAST> getBlock() { return std::move(Block); }
};

/**
  * Blockを表すAST
  */
class BlockAST {
  std::vector<std::unique_ptr<ConstDeclAST>> Constants;
  std::vector<std::unique_ptr<VarDeclAST>> Variables;
  //std::vector<std::unique_ptr<FuncDeclAST> Functions;

  public:
    BlockAST() {}
    ~BlockAST() {}
    void addConstant(std::unique_ptr<ConstDeclAST> constant);
    void addVariable(std::unique_ptr<VarDeclAST> variable);
    //bool addFunction(std::unique_ptr<FuncDeclAST> function);
    bool empty();
    std::vector<std::unique_ptr<ConstDeclAST>> getConstants() {
      return std::move(Constants);
    }
    std::vector<std::unique_ptr<VarDeclAST>> getVariables() {
      return std::move(Variables);
    }
};

/**
  * 定数定義を表すAST
  */
class ConstDeclAST {
private:
  std::map<std::string, int> NameValues;

public:
  ConstDeclAST() {}
  ~ConstDeclAST() {}
  void addConstant(std::string name, int value) {
    NameValues[name] = value;
  }
  std::map<std::string, int> getNameValues() { return NameValues; }
};

/**
  * 変数定義を表すAST
  */
class VarDeclAST {
private:
  std::vector<std::string> Names;

public:
  VarDeclAST() {}
  ~VarDeclAST() {}
  void addVariable(std::string name) { Names.push_back(name); }
  std::vector<std::string> getNames() { return Names; }
};

#endif
