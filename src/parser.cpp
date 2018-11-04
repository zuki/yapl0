#include "parser.hpp"

/**
  * コンストラクタ
  */
Parser::Parser(std::string filename) {
  Tokens = LexicalAnalysis(filename);
}

/**
  * 構文解析実効
  * @return 解析成功：true　解析失敗：false
  */
bool Parser::doParse() {
  if (!Tokens) {
    fprintf(stderr, "error at lexer\n");
    return false;
  } else
    return visitProgram();
}

/**
  * AST取得
  * @return TranslationUnitへの参照
  */
std::unique_ptr<ProgramAST> Parser::getAST() {
  return std::move(TheProgramAST);
}

// program:  block, '.'
/**
  * TranslationUnit用構文解析メソッド
  * @return 解析成功：true　解析失敗：false
  */
bool Parser::visitProgram() {
  // block
  std::unique_ptr<BlockAST> Block = visitBlock();
  if (Block->empty()) {
    fprintf(stderr, "error at visitBlock\n");
    return false;
  }

  // eat '.'
  if(!Tokens->isSymbol(".")) {
    fprintf(stderr, "Program don's end with '.'\n");
    return false;
  }

  TheProgramAST =  llvm::make_unique<ProgramAST>(std::move(Block));
  return true;
}

// block: { constDecl | varDecl | funcDecl }, statement
/**
  * Block用構文解析クラス
  * 解析したConstDeclASTとVarDeclAST, funcDeclASTをTheProgramASTに追加
  * @param TheProgramAST
  * @return true: 成功 false: 失敗
  */
std::unique_ptr<BlockAST> Parser::visitBlock() {
  auto Block = llvm::make_unique<BlockAST>();
  while (true) {
    bool c = false, v = false;
    auto ConstAST = visitConstDecl();
    if (ConstAST) {
      Block->addConstant(std::move(ConstAST));
      c = true;
    }
    auto VarAST = visitVarDecl();
    if (VarAST) {
      Block->addVariable(std::move(VarAST));
      v = true;
    }

/*
    std::unique_ptr<FuncDeclAST> BlockAST = visitFuncDecl();
    if (BlockAST)
      BlockAST->addFunction(std::move(func));
    if (!ConstAST && !VarAST && !func)
*/
    if (!c && !v)
      break;
  }

  return Block;
}


// constDecl: 'const', ident, '=', number, { ',' , ident, '=', number }, ';'
/**
  * ConstDect用構文解析メソッド
  * @return 成功: std::unique_ptr<ConstDeclAST>, 失敗: nullptr
  */
std::unique_ptr<ConstDeclAST> Parser::visitConstDecl() {
  std::string name;
  // TODO: 変数重複チェック
  int bkup = Tokens->getCurIndex();
  auto ConstAST = llvm::make_unique<ConstDeclAST>();

  if (Tokens->getCurType() != TOK_CONST)
    return nullptr;

  // eat 'const'
  Tokens->getNextToken();
  name = Tokens->getCurString();
  // eat ident
  Tokens->getNextToken();
  if (!Tokens->isSymbol("=")) {
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  // eat '='
  Tokens->getNextToken();
  ConstAST->addConstant(name, Tokens->getCurNumVal());
  // eat number
  Tokens->getNextToken();
  if (Tokens->isSymbol(",")) {
    // eat ','
    Tokens->getNextToken();
    while(1) {
      name = Tokens->getCurString();
      // eat ident
      Tokens->getNextToken();
      if (!Tokens->isSymbol("=")) {
        Tokens->applyTokenIndex(bkup);
        return nullptr;
      }
      // eat '='
      Tokens->getNextToken();
      ConstAST->addConstant(name, Tokens->getCurNumVal());
      // eat number
      Tokens->getNextToken();
      if (!Tokens->isSymbol(","))
        break;
    }
  }
  if (!Tokens->isSymbol(";")) {
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  // eat ';'
  Tokens->getNextToken();
  return ConstAST;
}

// varDecl: 'var', ident, { ',', ident }, ';'
/**
  * VarDecl用構文解析メソッド
  * @return 成功: std::unique_ptr<VarDeclAST>, 失敗: nullptr
  */
std::unique_ptr<VarDeclAST> Parser::visitVarDecl() {
  int bkup = Tokens->getCurIndex();
  auto VarAST = llvm::make_unique<VarDeclAST>();

  if (Tokens->getCurType() != TOK_VAR)
    return nullptr;

  // eat 'var'
  Tokens->getNextToken();
  VarAST->addVariable(Tokens->getCurString());
  // eat ident
  Tokens->getNextToken();
  if (Tokens->isSymbol(",")) {
    // eat ','
    Tokens->getNextToken();
    while(1) {
      VarAST->addVariable(Tokens->getCurString());
      // eat ident
      Tokens->getNextToken();
      if (!Tokens->isSymbol(","))
        break;
    }
  }
  if (!Tokens->isSymbol(";")) {
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  // eat ';'
  Tokens->getNextToken();

  return VarAST;
}
