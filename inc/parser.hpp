#ifndef PARSER_HPP
#define PARSER_HPP

#include "llvm/ADT/STLExtras.h"
#include<algorithm>
#include<cstdio>
#include<cstdlib>
#include<map>
#include<string>
#include<vector>
#include"ast.hpp"
#include"lexer.hpp"
//using namespace llvm;

/**
  * nameの種類
  */
enum NameType {
  CONST,
  VAR,
  PARAM,
  FUNC,
  TEMP
};

typedef struct TableEntry {
  int level;
  NameType type;
  std::string name;
  int num;        // Func: 引数の数

  TableEntry(int p_level, NameType p_type, std::string p_name, int p_num): level(p_level),type(p_type),name(p_name),num(p_num){}
} TableEntry;

/**
  * 構文解析・意味解析クラス
  */
typedef class Parser{
private:
  int CurrentLevel = -1;
  bool Debug;
  std::unique_ptr<TokenStream> Tokens;
  std::unique_ptr<ProgramAST> TheProgramAST;

  //意味解析用各種識別子表
  std::vector<TableEntry> SymbolTable;      // 名前シンボルテーブル

public:
  Parser(std::string filename, bool debug);
  ~Parser() {}
  bool doParse();
  std::unique_ptr<ProgramAST> getAST();

private:
  /**
    各種構文解析メソッド
    */
  void blockIn() { CurrentLevel++; }
  void blockOut() {
    while (!SymbolTable.empty() && SymbolTable.back().level == CurrentLevel) {
      SymbolTable.pop_back();
    }
    CurrentLevel--;
  }
  bool visitProgram();
  std::unique_ptr<BlockAST> visitBlock();
  std::unique_ptr<ConstDeclAST> visitConstDecl();
  std::unique_ptr<VarDeclAST> visitVarDecl();
  std::unique_ptr<FuncDeclAST> visitFuncDecl();
  std::unique_ptr<BaseStmtAST> visitStatement();
  std::unique_ptr<BaseStmtAST> visitAssign();
  std::unique_ptr<BaseStmtAST> visitBeginEnd();
  std::unique_ptr<BaseStmtAST> visitIfThen();
  std::unique_ptr<BaseStmtAST> visitWhileDo();
  std::unique_ptr<BaseStmtAST> visitReturn();
  std::unique_ptr<BaseStmtAST> visitWrite();
  std::unique_ptr<BaseExpAST> visitCondition();
  std::unique_ptr<BaseExpAST> visitExpression(std::unique_ptr<BaseExpAST> lhs);
  std::unique_ptr<BaseExpAST> visitTerm(std::unique_ptr<BaseExpAST> lhs);
  std::unique_ptr<BaseExpAST> visitFactor();
  std::unique_ptr<BaseExpAST> visitCall(const std::string &name);
  void debug_check(const std::string method);
  void addSymbol(std::string name, NameType type, int num);
  void addSymbol(std::string name, NameType type) {
    addSymbol(name, type, -1);
  }
  void deleteTemp(int pos);
  int findSymbol(std::string name, NameType type, bool checkLevel, int num);
  int findSymbol(std::string name, NameType type) {
    return findSymbol(name, type, true, -1);
  }
  bool remainedTemp();
  int getLevel() { return CurrentLevel; }
} Parser;

#endif
