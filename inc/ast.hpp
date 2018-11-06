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
class BaseExpAST;
class BaseStmtAST;
class ProgramAST;
class BlockAST;
class ConstDeclAST;
class VarDeclAST;
class FuncDeclAST;
class StatmentAST;
class NullAST;
class AssignAST;
class BeginEndAST;
class IfThenAST;
class WhileDoAST;
class ReturnAST;
class WriteAST;
class WritelnAST;
class CondExpAST;
class BinaryExprAST;
class VariableAST;
class NumberAST;
class CallExprAST;

/**
  * ASTの種類
  */
enum AstID {
// 式
  BaseExpID,
  CondExpID,
  BinaryExprID,
  CallExprID,
  VariableID,
  NumberID,
// 文
  BaseStmtID,
  NullID,
  AssignID,
  BeginEndID,
  IfThenID,
  WhileDoID,
  ReturnID,
  WriteID,
  WritelnID,
};


/**
  * 式のASTの基底クラス
  */
class BaseExpAST {
  AstID ID;

  public:
  BaseExpAST(AstID id): ID(id) {}
  virtual ~BaseExpAST() {}
  AstID getValueID() const { return ID; }
};

/**
  * 文のASTの基底クラス
  */
class BaseStmtAST {
  AstID ID;

  public:
  BaseStmtAST(AstID id): ID(id) {}
  virtual ~BaseStmtAST() {}
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
  std::vector<std::unique_ptr<FuncDeclAST>> Functions;
  std::unique_ptr<BaseStmtAST> Statement;

  public:
    BlockAST() {}
    ~BlockAST() {}
    void addConstant(std::unique_ptr<ConstDeclAST> constant);
    void addVariable(std::unique_ptr<VarDeclAST> variable);
    void addFunction(std::unique_ptr<FuncDeclAST> function);
    void addStatement(std::unique_ptr<BaseStmtAST> statement);
    bool empty();
    std::vector<std::unique_ptr<ConstDeclAST>> getConstants() {
      return std::move(Constants);
    }
    std::vector<std::unique_ptr<VarDeclAST>> getVariables() {
      return std::move(Variables);
    }
    std::vector<std::unique_ptr<FuncDeclAST>> getFunctions() {
      return std::move(Functions);
    }
    std::unique_ptr<BaseStmtAST> getStatement() {
      return std::move(Statement);
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
  void addConstant(const std::string &name, int value) {
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
  void addVariable(const std::string &name) { Names.push_back(name); }
  std::vector<std::string> getNames() { return Names; }
};

/**
  * 関数定義を表すAST
  */
class FuncDeclAST {
private:
  std::string Name;
  std::vector<std::string> Parameters;
  std::unique_ptr<BlockAST> Block;

public:
  FuncDeclAST(const std::string &name, std::vector<std::string> parameters, std::unique_ptr<BlockAST> block):
    Name(name), Parameters(parameters), Block(std::move(block)) {}
  ~FuncDeclAST() {}
  std::string getName() { return Name; }
  std::vector<std::string> getParameters() { return Parameters; }
  std::unique_ptr<BlockAST> getBlock() { return std::move(Block); }
};

/**
  * 文を表すAST
  */
/*
class StatementAST : BaseStmtAST {
private:
  std::vector<BaseStmtAST> Statements;

public:
  StatementAST(): BaseStmetAST() {}
  ~StatementAST() {}
  std::vector<BaseStmtAST> getStatements() { return Statements; }
  void addStatement(std::unique_ptr<BaseStmtAST> statement) {
    Statements.push_back(std::move(statement));
  }
};
*/

/**
  * 空文を表すAST
  */
class NullAST : public BaseStmtAST {
public:
  NullAST() : BaseStmtAST(NullID) {}
  ~NullAST() {}
  static inline bool classof(NullAST const*) { return true; }
  static inline bool classof(BaseStmtAST const* base) {
     return base->getValueID() == NullID;
  }
};

/**
  * 代入文を表すAST
  */
class AssignAST : public BaseStmtAST {
private:
  std::string Name;
  std::unique_ptr<BaseExpAST> RHS;

public:
  AssignAST(const std::string &name, std::unique_ptr<BaseExpAST> rhs) : BaseStmtAST(AssignID), Name(name), RHS(std::move(rhs)) {}
  ~AssignAST() {}
  static inline bool classof(AssignAST const*) { return true; }
  static inline bool classof(BaseStmtAST const* base) {
     return base->getValueID() == AssignID;
  }
  std::unique_ptr<BaseExpAST> getRHS() {
    return std::move(RHS);
  }
};

/**
  * Begin/End文を表すAST
  */
class BeginEndAST : public BaseStmtAST {
private:
  std::vector<std::unique_ptr<BaseStmtAST>> Statements;

public:
  BeginEndAST(std::vector<std::unique_ptr<BaseStmtAST>> statements) : BaseStmtAST(BeginEndID), Statements(std::move(statements)) {}
  ~BeginEndAST() {}
  static inline bool classof(BeginEndAST const*) { return true; }
  static inline bool classof(BaseStmtAST const* base) {
     return base->getValueID() == BeginEndID;
  }
  std::vector<std::unique_ptr<BaseStmtAST>> getStatements() {
    return std::move(Statements);
  }
};

/**
  * If/Then文を表すAST
  */
class IfThenAST : public BaseStmtAST {
private:
  std::unique_ptr<BaseExpAST> Condition;
  std::unique_ptr<BaseStmtAST> Statement;

public:
  IfThenAST(std::unique_ptr<BaseExpAST> condition, std::unique_ptr<BaseStmtAST> statement) :
    BaseStmtAST(IfThenID), Condition(std::move(condition)), Statement(std::move(statement)) {}
  ~IfThenAST() {}
  static inline bool classof(IfThenAST const*) { return true; }
  static inline bool classof(BaseStmtAST const* base) {
     return base->getValueID() == IfThenID;
  }
  std::unique_ptr<BaseExpAST> getCondition() {
    return std::move(Condition);
  }
  std::unique_ptr<BaseStmtAST> getStatement() {
    return std::move(Statement);
  }
};

/**
  * While/Do文を表すAST
  */
class WhileDoAST : public BaseStmtAST {
private:
  std::unique_ptr<BaseExpAST> Condition;
  std::unique_ptr<BaseStmtAST> Statement;

public:
  WhileDoAST(std::unique_ptr<BaseExpAST> condition, std::unique_ptr<BaseStmtAST> statement) :
    BaseStmtAST(WhileDoID), Condition(std::move(condition)), Statement(std::move(statement)) {}
  ~WhileDoAST() {}
  static inline bool classof(WhileDoAST const*) { return true; }
  static inline bool classof(BaseStmtAST const* base) {
     return base->getValueID() == WhileDoID;
  }
  std::unique_ptr<BaseExpAST> getCondition() {
    return std::move(Condition);
  }
  std::unique_ptr<BaseStmtAST> getStatement() {
    return std::move(Statement);
  }
};

/**
  * リターン文を表すAST
  */
class ReturnAST : public BaseStmtAST {
private:
  std::unique_ptr<BaseExpAST> Expression;

public:
  ReturnAST(std::unique_ptr<BaseExpAST> expression) :
    BaseStmtAST(ReturnID), Expression(std::move(expression)) {}
  ~ReturnAST() {}
  static inline bool classof(ReturnAST const*) { return true; }
  static inline bool classof(BaseStmtAST const* base) {
     return base->getValueID() == ReturnID;
  }
  std::unique_ptr<BaseExpAST> getExpression() {
    return std::move(Expression);
  }
};

/**
  * Write文を表すAST
  */
class WriteAST : public BaseStmtAST {
private:
  std::unique_ptr<BaseExpAST> Expression;

public:
  WriteAST(std::unique_ptr<BaseExpAST> expression) :
    BaseStmtAST(WriteID), Expression(std::move(expression)) {}
  ~WriteAST() {}
  static inline bool classof(WriteAST const*) { return true; }
  static inline bool classof(BaseStmtAST const* base) {
     return base->getValueID() == WriteID;
  }
  std::unique_ptr<BaseExpAST> getExpression() {
    return std::move(Expression);
  }
};

/**
  * Writeln文を表すAST
  */
class WritelnAST : public BaseStmtAST {
public:
  WritelnAST() : BaseStmtAST(WritelnID) {}
  ~WritelnAST() {}
  static inline bool classof(WritelnAST const*) { return true; }
  static inline bool classof(BaseStmtAST const* base) {
     return base->getValueID() == WritelnID;
  }
};

/**
  * 条件式を表すAST
  */
class CondExpAST : public BaseExpAST {
private:
  std::string Op;
  std::unique_ptr<BaseExpAST> LHS, RHS;

public:
  CondExpAST(const std::string& op, std::unique_ptr<BaseExpAST> lhs, std::unique_ptr<BaseExpAST> rhs) :
    BaseExpAST(CondExpID), Op(op), LHS(std::move(lhs)), RHS(std::move(rhs)) {}
  ~CondExpAST() {}
  static inline bool classof(CondExpAST const*) { return true; }
  static inline bool classof(BaseExpAST const* base) {
     return base->getValueID() == CondExpID;
  }
  std::string getOp() { return Op; }
  std::unique_ptr<BaseExpAST> getLHS() { return std::move(LHS); }
  std::unique_ptr<BaseExpAST> getRHS() { return std::move(RHS); }
};

/**
  * 二項演算式を表すAST
  */
class BinaryExprAST : public BaseExpAST {
private:
  std::string Op;
  std::unique_ptr<BaseExpAST> LHS, RHS;
  std::string Prefix;

public:
  BinaryExprAST(const std::string& op, std::unique_ptr<BaseExpAST> lhs, std::unique_ptr<BaseExpAST> rhs, const std::string& prefix = "") :
    BaseExpAST(BinaryExprID), Op(op), LHS(std::move(lhs)), RHS(std::move(rhs)), Prefix(prefix)  {}
  ~BinaryExprAST() {}
  static inline bool classof(BinaryExprAST const*) { return true; }
  static inline bool classof(BaseExpAST const* base) {
     return base->getValueID() == BinaryExprID;
  }
  std::string getOp() { return Op; }
  std::string getPrefix() { return Prefix; }
  std::unique_ptr<BaseExpAST> getLHS() { return std::move(LHS); }
  std::unique_ptr<BaseExpAST> getRHS() { return std::move(RHS); }
};

/**
  * 関数呼び出しを表すAST
  */
class CallExprAST : public BaseExpAST {
private:
  std::string Callee;
  std::vector<std::unique_ptr<BaseExpAST>> Args;

public:
  CallExprAST(const std::string &callee)
    : BaseExpAST(CallExprID), Callee(callee) {}
  ~CallExprAST() {}
  std::string getCallee() { return Callee; }
  std::unique_ptr<BaseExpAST> getArgs(unsigned i) {
    if (i < Args.size()) return std::move(Args.at(i));
    else return nullptr;
  }
  static inline bool classof(CallExprAST const*) { return true; }
  static inline bool classof(BaseExpAST const* base) {
    return base->getValueID() == CallExprID;
  }
  void addArg(std::unique_ptr<BaseExpAST> arg) {
    Args.push_back(std::move(arg));
  }
};

/**
  * 変数参照式を表すAST
  */
class VariableAST : public BaseExpAST{
private:
  std::string Name;

public:
  VariableAST(const std::string &name) : BaseExpAST(VariableID), Name(name) {}
  ~VariableAST(){}
  static inline bool classof(VariableAST const*) { return true; }
  static inline bool classof(BaseExpAST const* base) {
    return base->getValueID() == VariableID;
  }
  std::string getName() { return Name; }
};


/**
  * 整数式を表すAST
  */
class NumberAST : public BaseExpAST {
private:
  int Val;

public:
  NumberAST(int val) : BaseExpAST(NumberID), Val(val) {};
  ~NumberAST() {}
  int getNumberValue() { return Val; }
  static inline bool classof(NumberAST const*){ return true; }
  static inline bool classof(BaseExpAST const* base) {
    return base->getValueID() == NumberID;
  }
};

#endif
