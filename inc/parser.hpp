#ifndef PARSER_HPP
#define PARSER_HPP

#include "llvm/ADT/STLExtras.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <map>
#include <string>
#include <vector>
#include "ast.hpp"
#include "lexer.hpp"
#include "table.hpp"
//using namespace llvm;

/**
  * 構文解析・意味解析クラス
  */
class Parser{
private:
  static const int MINERROR = 30;
  bool Debug;
  std::unique_ptr<TokenStream> Tokens;
  std::unique_ptr<ProgramAST> TheProgramAST;

  //意味解析用各種識別子表
  SymTable sym_table;      // 名前シンボルテーブル

public:
  Parser(std::string filename, bool debug);
  ~Parser() {}
  bool parse();
  std::unique_ptr<ProgramAST> getAST();

private:
  /**
    各種構文解析メソッド
    */
  bool parseProgram();
  std::unique_ptr<BlockAST> parseBlock();
  std::unique_ptr<ConstDeclAST> parseConst();
  std::unique_ptr<VarDeclAST> parseVar();
  std::unique_ptr<FuncDeclAST> parseFunction();
  std::unique_ptr<BaseStmtAST> parseStatement();
  std::unique_ptr<BaseStmtAST> parseAssign();
  std::unique_ptr<BaseStmtAST> parseBeginEnd();
  std::unique_ptr<BaseStmtAST> parseIfThen();
  std::unique_ptr<BaseStmtAST> parseWhileDo();
  std::unique_ptr<BaseStmtAST> parseReturn();
  std::unique_ptr<BaseStmtAST> parseWrite();
  std::unique_ptr<BaseExpAST> parseCondition();
  std::unique_ptr<BaseExpAST> parseExpression(std::unique_ptr<BaseExpAST> lhs);
  std::unique_ptr<BaseExpAST> parseTerm(std::unique_ptr<BaseExpAST> lhs);
  std::unique_ptr<BaseExpAST> parseFactor();
  std::unique_ptr<BaseExpAST> parseCall(const std::string &name, Token token);
  void checkGet(std::string symbol);
  void check(const std::string &caller);
  bool isKeyWord(std::string &name);
  bool isSymbol(std::string &name);
  bool isKeyWordType(int type);
  bool isStmtBeginKey(const std::string &name);
};

#endif
