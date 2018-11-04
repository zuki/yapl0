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
  * 構文解析・意味解析クラス
  */
typedef class Parser{
private:
  std::unique_ptr<TokenStream> Tokens;
  std::unique_ptr<ProgramAST> TheProgramAST;

  //意味解析用各種識別子表
  std::map<std::string, int> ConstantTable;    // 定数名 => 値
  std::vector<std::string> VariableTable;      // 変数名
  std::map<std::string, int> FunctionTable;    // 関数名 => 引数個数

public:
  Parser(std::string filename);
  ~Parser() {}
  bool doParse();
  std::unique_ptr<ProgramAST> getAST();

private:
  /**
    各種構文解析メソッド
    */
  bool visitProgram();
  std::unique_ptr<BlockAST> visitBlock();
  std::unique_ptr<ConstDeclAST> visitConstDecl();
  std::unique_ptr<VarDeclAST> visitVarDecl();
} Parser;

#endif
