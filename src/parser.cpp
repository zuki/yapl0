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
  check("visitProgram");
  // block
  std::unique_ptr<BlockAST> Block = visitBlock();
  if (!Block || Block->empty()) {
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
  check("visitBlock");
  auto Block = llvm::make_unique<BlockAST>();
  while (true) {
    // std::moveするとuniq_ptrはnullになってしまうので
    // 別にフラグが必要
    bool c = false, v = false, f = false;

    auto const_decl = visitConstDecl();
    if (const_decl) {
      Block->addConstant(std::move(const_decl));
      c = true;
    }

    auto var_decl = visitVarDecl();
    if (var_decl) {
      Block->addVariable(std::move(var_decl));
      v = true;
    }

    auto func_decl = visitFuncDecl();
    if (func_decl) {
      Block->addFunction(std::move(func_decl));
      f = true;
    }

    if (!c && !v && !f)
      break;
  }

  auto statement = visitStatement();
  if (statement) {
    Block->addStatement(std::move(statement));
  } else {
    fprintf(stderr, "No statement\n");
    return nullptr;
  }

  return Block;
}


// constDecl: 'const', ident, '=', number, { ',' , ident, '=', number }, ';'
/**
  * ConstDect用構文解析メソッド
  * @return 成功: std::unique_ptr<ConstDeclAST>, 失敗: nullptr
  */
std::unique_ptr<ConstDeclAST> Parser::visitConstDecl() {
  check("visitConstDecl");
  std::string name;
  // TODO: 変数重複チェック
  int bkup = Tokens->getCurIndex();
  auto ConstAST = llvm::make_unique<ConstDeclAST>();

  if (Tokens->getCurType() != TOK_CONST)
    return nullptr;

  Tokens->getNextToken(); // eat 'const'
  name = Tokens->getCurString();
  Tokens->getNextToken();   // eat ident
  if (!Tokens->isSymbol("=")) {
    fprintf(stderr, "visitConstDecl: expected '=' but %s\n", Tokens->getCurString().c_str());
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  Tokens->getNextToken(); // eat '='
  ConstAST->addConstant(name, Tokens->getCurNumVal());
  Tokens->getNextToken(); // eat number
  while (Tokens->isSymbol(",")) {
    Tokens->getNextToken(); // eat ','
    name = Tokens->getCurString();
    Tokens->getNextToken(); // eat ident
    if (!Tokens->isSymbol("=")) {
      fprintf(stderr, "visitConstDecl: expected '=' but %s\n", Tokens->getCurString().c_str());
      Tokens->applyTokenIndex(bkup);
      return nullptr;
    }
    Tokens->getNextToken(); // eat '='
    ConstAST->addConstant(name, Tokens->getCurNumVal());
    Tokens->getNextToken(); // eat number
  }
  if (!Tokens->isSymbol(";")) {
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  Tokens->getNextToken(); // eat ';'

  return ConstAST;
}

// varDecl: 'var', ident, { ',', ident }, ';'
/**
  * VarDecl用構文解析メソッド
  * @return 成功: std::unique_ptr<VarDeclAST>, 失敗: nullptr
  */
std::unique_ptr<VarDeclAST> Parser::visitVarDecl() {
  check("visitVarDecl");
  int bkup = Tokens->getCurIndex();
  auto VarAST = llvm::make_unique<VarDeclAST>();

  if (Tokens->getCurType() != TOK_VAR)
    return nullptr;

  // eat 'var'
  Tokens->getNextToken();
  VarAST->addVariable(Tokens->getCurString());
  // eat ident
  Tokens->getNextToken();
  while(Tokens->isSymbol(",")) {
    Tokens->getNextToken(); // eat ','
    VarAST->addVariable(Tokens->getCurString());
    Tokens->getNextToken(); // eat ident
  }
  if (!Tokens->isSymbol(";")) {
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  // eat ';'
  Tokens->getNextToken();

  return VarAST;
}

// funcDecl: ''function', ident, '(', [ ident, { ',', ident } ], ')', block, ';'
/**
  * FuncDecl用構文解析メソッド
  * @return 成功: std::unique_ptr<FuncDeclAST>, 失敗: nullptr
  */
std::unique_ptr<FuncDeclAST> Parser::visitFuncDecl() {
  check("visitFuncDecl");
  int bkup = Tokens->getCurIndex();
  std::string name;
  std::vector<std::string> parameters;

  if (Tokens->getCurType() != TOK_FUNCTION)
    return nullptr;

  Tokens->getNextToken(); // eat 'function'
  name = Tokens->getCurString();
  Tokens->getNextToken(); // eat ident
  if (!Tokens->isSymbol("(")) {
    fprintf(stderr, "visitFuncDecl: expected '(' but %s\n", Tokens->getCurString().c_str());
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  Tokens->getNextToken(); // eat '('
  bool is_first_param = true;
  while(true){
    if (!is_first_param && Tokens->isSymbol(",")) {
      Tokens->getNextToken(); // eat ','
    }
    if (Tokens->getCurType() == TOK_IDENTIFIER) {
      parameters.push_back(Tokens->getCurString());
      Tokens->getNextToken(); // eat ident
    } else {
      break; // 最初に識別子が来なければparamtersは終了
    }
    is_first_param = false;
  }
  if (!Tokens->isSymbol(")")) {
    fprintf(stderr, "visitFuncDecl: expected ')' but %s\n", Tokens->getCurString().c_str());
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  Tokens->getNextToken(); // eat ')'

  auto block = visitBlock();
  if (!block) {
    fprintf(stderr, "visitFuncDecl: error at block\n");
    return nullptr;
  }

  if (!Tokens->isSymbol(";")) {
    fprintf(stderr, "visitFuncDecl: expected ';' but %s\n", Tokens->getCurString().c_str());
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  Tokens->getNextToken(); // eat ';'

  return llvm::make_unique<FuncDeclAST>(name, parameters, std::move(block));
}

// statment
/**
  * Statement用構文解析メソッド
  * @return 成功: std::unique_ptr<StatementAST>, 失敗: nullptr
  */
std::unique_ptr<BaseStmtAST> Parser::visitStatement() {
  check("visitStatement");
  int bkup = Tokens->getCurIndex();
  std::unique_ptr<BaseStmtAST> statement;

  switch (Tokens->getCurType()) {
    case TOK_IDENTIFIER:
      statement = visitAssign();
      break;
    case TOK_BEGIN:
      statement = visitBeginEnd();
      break;
    case TOK_IF:
      statement = visitIfThen();
      break;
    case TOK_WHILE:
      statement = visitWhileDo();
      break;
    case TOK_RETURN:
      statement = visitReturn();
      break;
    case TOK_WRITE:
      statement = visitWrite();
      break;
    case TOK_WRITELN:
      statement = llvm::make_unique<WritelnAST>();
      Tokens->getNextToken();   // eat 'writeln'
      break;
    default:
      if (Tokens->isSymbol(".") || Tokens->getCurType() == TOK_END) {
        statement = llvm::make_unique<NullAST>();
      } else if (Tokens->isSymbol(";")) {
        statement = llvm::make_unique<NullAST>();
          Tokens->getNextToken();
      } else {
        fprintf(stderr, "visitStatement: unexpected token: %s\n", Tokens->getCurString().c_str());
        Tokens->applyTokenIndex(bkup);
        return nullptr;
      }
  }

  return statement;
}

// ident ':=' expression
/**
  * FuncDecl用構文解析メソッド
  * @return 成功: std::unique_ptr<BaseStmtAST>, 失敗: nullptr
  */
std::unique_ptr<BaseStmtAST> Parser::visitAssign() {
  check("visitAssign");
  int bkup = Tokens->getCurIndex();
  std::string name;

  name = Tokens->getCurString();
  Tokens->getNextToken(); // eat ident
  if (Tokens->getCurType() != TOK_ASSIGN) {
    fprintf(stderr, "visitAssign: expect ':=', but %s\n", Tokens->getCurString().c_str());
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  Tokens->getNextToken(); // eat ':='
  auto rhs = visitExpression(nullptr);
  if (!rhs) {
    fprintf(stderr, "visitAssign: error getting expression\n");
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }

  return llvm::make_unique<AssignAST>(name, std::move(rhs));
}

// 'begin' statement { ';' statement } 'end'
/**
  * BeginEnd用構文解析メソッド
  * @return 成功: std::unique_ptr<BaseStmtAST>, 失敗: nullptr
  */
std::unique_ptr<BaseStmtAST> Parser::visitBeginEnd() {
  check("visitBeginEnd");
  int bkup = Tokens->getCurIndex();
  std::unique_ptr<BaseStmtAST> statement;
  std::vector<std::unique_ptr<BaseStmtAST>> statements;

  Tokens->getNextToken(); // eat 'begin'
  statement = visitStatement();
  if (!statement) {
    return nullptr;
  }
  statements.push_back(std::move(statement));
  while (Tokens->isSymbol(";")) {
    Tokens->getNextToken();  // eat ';'
    statement = visitStatement();
    if (!statement) {
      return nullptr;
    }
    statements.push_back(std::move(statement));
  }
  if (Tokens->getCurType() != TOK_END) {
    fprintf(stderr, "visitBeginEnd: expect 'end', but %s\n", Tokens->getCurString().c_str());
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  Tokens->getNextToken();  // eat 'end'

  return llvm::make_unique<BeginEndAST>(std::move(statements));
}

// 'if' condition 'then' statement
/**
  * BeginEnd用構文解析メソッド
  * @return 成功: std::unique_ptr<BaseStmtAST>, 失敗: nullptr
  */
std::unique_ptr<BaseStmtAST> Parser::visitIfThen() {
  check("visitIfThen");
  int bkup = Tokens->getCurIndex();

  Tokens->getNextToken(); // eat 'if'
  auto condition = visitCondition();
  if (!condition) {
    return nullptr;
  }
  if (Tokens->getCurType() != TOK_THEN) {
    fprintf(stderr, "visitAssign: expect 'then', but %s\n", Tokens->getCurString().c_str());
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  Tokens->getNextToken(); // eat 'then'
  auto statement = visitStatement();
  if (!statement) {
    return nullptr;
  }
  return llvm::make_unique<IfThenAST>(std::move(condition), std::move(statement));
}

// 'while' condition 'do' statment
/**
  * WhileDo用構文解析メソッド
  * @return 成功: std::unique_ptr<BaseStmtAST>, 失敗: nullptr
  */
std::unique_ptr<BaseStmtAST> Parser::visitWhileDo() {
  check("visitWhileDo");
  int bkup = Tokens->getCurIndex();

  Tokens->getNextToken(); // eat 'while'
  auto condition = visitCondition();
  if (!condition) {
    return nullptr;
  }
  if (Tokens->getCurType() != TOK_DO) {
    fprintf(stderr, "visitAssign: expect 'do', but %s\n", Tokens->getCurString().c_str());
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  Tokens->getNextToken(); // eat 'do'
  auto statement = visitStatement();
  if (!statement) {
    return nullptr;
  }
  return llvm::make_unique<WhileDoAST>(std::move(condition), std::move(statement));
}

// 'return' expression
/**
  * Return用構文解析メソッド
  * @return 成功: std::unique_ptr<BaseStmtAST>, 失敗: nullptr
  */
std::unique_ptr<BaseStmtAST> Parser::visitReturn() {
  check("visitReturn");

  Tokens->getNextToken(); // eat 'return'
  auto expression = visitExpression(nullptr);
  if (!expression) {
    return nullptr;
  }
  return llvm::make_unique<ReturnAST>(std::move(expression));
}

// 'write' expression
/**
  * Write用構文解析メソッド
  * @return 成功: std::unique_ptr<BaseStmtAST>, 失敗: nullptr
  */
std::unique_ptr<BaseStmtAST> Parser::visitWrite() {
  check("visitWrite");

  Tokens->getNextToken(); // eat 'write'
  auto expression = visitExpression(nullptr);
  if (!expression) {
    return nullptr;
  }
  return llvm::make_unique<WriteAST>(std::move(expression));
}

// condition: 'odd' expression | expression () expression
/**
  * Write用構文解析メソッド
  * @return 成功: std::unique_ptr<BaseExpAST>, 失敗: nullptr
  */
std::unique_ptr<BaseExpAST> Parser::visitCondition() {
  check("visitCondition");
  int bkup=Tokens->getCurIndex();

  if (Tokens->getCurType() == TOK_ODD) {
    Tokens->getNextToken(); // eat 'odd'
    auto rhs = visitExpression(nullptr);
    if (!rhs) {
      return nullptr;
    }
    return llvm::make_unique<CondExpAST>("odd", nullptr, std::move(rhs));
  }

  auto lhs = visitExpression(nullptr);
  if (!lhs) {
    return nullptr;
  }
  std::string op = Tokens->getCurString();
  if (!(op == "=" || op == "<>" || op == "<" ||
        op == ">" || op == "<=" || op == ">=")) {
    fprintf(stderr, "visitCondition: unexpected token %s\n", Tokens->getCurString().c_str());
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  Tokens->getNextToken(); // eat Symbol
  auto rhs = visitExpression(nullptr);
  if (!rhs) {
    return nullptr;
  }

  return llvm::make_unique<CondExpAST>(op, std::move(lhs), std::move(rhs));
}

// expression: [ ( '+' | '-' ) ] term { ('+' | '-') term }
/**
  * Expression用構文解析メソッド
  * @return 成功: std::unique_ptr<BaseExpAST>, 失敗: nullptr
  */
std::unique_ptr<BaseExpAST> Parser::visitExpression(std::unique_ptr<BaseExpAST> lhs) {
  check("visitExpression");
  int bkup=Tokens->getCurIndex();
  std::string prefix = "";
  std::string op;

  if (Tokens->isSymbol("+") || Tokens->isSymbol("-")) {
    prefix = Tokens->getCurString();
    Tokens->getNextToken();  // eat prefix
  }

  if (!lhs)
    lhs = visitTerm(nullptr);
  if (!lhs) {
    fprintf(stderr, "visitExpression: no lhs expression\n");
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }

  if (Tokens->isSymbol("+") || Tokens->isSymbol("-")) {
    op = Tokens->getCurString();
    Tokens->getNextToken(); // eat '+' of '-'
    auto rhs = visitTerm(nullptr);
    if (rhs) {
      return visitExpression(llvm::make_unique<BinaryExprAST>(op, std::move(lhs), std::move(rhs), prefix));
    } else {
      fprintf(stderr, "visitExpression: no rhs expression\n");
      Tokens->applyTokenIndex(bkup);
      return nullptr;
    }
  }

  return lhs;
}

// term: factor, { ('*' | '/' ), factor }
/**
  * Term用構文解析メソッド
  * @return 成功: std::unique_ptr<BaseExpAST>, 失敗: nullptr
  */
std::unique_ptr<BaseExpAST> Parser::visitTerm(std::unique_ptr<BaseExpAST> lhs) {
  check("visitTerm");
  int bkup=Tokens->getCurIndex();
  std::string op;

  if (!lhs)
    lhs = visitFactor();
  if (!lhs) {
    fprintf(stderr, "visitTerm: no lhs expression\n");
    return nullptr;
  }

  if (Tokens->isSymbol("*") || Tokens->isSymbol("/")) {
    op = Tokens->getCurString();
    Tokens->getNextToken(); // eat '*' or '/'
    auto rhs = visitFactor();
    if (rhs) {
      return visitTerm(llvm::make_unique<BinaryExprAST>(op, std::move(lhs), std::move(rhs)));
    } else {
      fprintf(stderr, "visitTerm: no rhs expression\n");
      Tokens->applyTokenIndex(bkup);
      return nullptr;
    }
  }

  return lhs;
}

// factor: ident | number | '(' expression ')' |
//         ident '(' [ expression, { ',' expression } ]
/**
  * Factor用構文解析メソッド
  * @return 成功: std::unique_ptr<BaseExpAST>, 失敗: nullptr
  */
std::unique_ptr<BaseExpAST> Parser::visitFactor() {
  check("visitFactor");
  int bkup=Tokens->getCurIndex();

  // identifier: 定義済み変数
  if (Tokens->getCurType() == TOK_IDENTIFIER) {
    std::string name = Tokens->getCurString();
    Tokens->getNextToken(); // eat ident
    if (Tokens->isSymbol("(")) {
      return visitCall(name);
    }
    return llvm::make_unique<VariableAST>(name);
  } else if (Tokens->getCurType() == TOK_DIGIT) {
    int val=Tokens->getCurNumVal();
    Tokens->getNextToken(); // eat digit
    return llvm::make_unique<NumberAST>(val);
  } else if (Tokens->isSymbol("(")) {
    Tokens->getNextToken(); // eat '('
    auto expr = visitExpression(nullptr);
    if (!expr) {
      fprintf(stderr, "visitFactor: no expression in paren\n");
      Tokens->applyTokenIndex(bkup);
      return nullptr;
    }
    if(!Tokens->isSymbol(")")) {
      fprintf(stderr, "visitFactor: expect ')' but %s\n",
        Tokens->getCurString().c_str());
      Tokens->applyTokenIndex(bkup);
      return nullptr;
    }
    Tokens->getNextToken(); // eat ')'
    return expr;
  }
  return nullptr;
}

// factor: '(' expression ')' |
/**
  * Factor(関数呼び出し)用構文解析メソッド
  * @return 成功: std::unique_ptr<BaseExpAST>, 失敗: nullptr
  */
std::unique_ptr<BaseExpAST> Parser::visitCall(const std::string &callee) {
  check("visitCall");
  int bkup=Tokens->getCurIndex();

  auto call_expr = llvm::make_unique<CallExprAST>(callee);
  Tokens->getNextToken(); // eat '('
  auto arg = visitExpression(nullptr);
  if (arg) {
    while (true) {
      call_expr->addArg(std::move(arg));
      if (!Tokens->isSymbol(","))
        break;
      Tokens->getNextToken(); // eat ','
      arg = visitExpression(nullptr);
    }
  }
  if (!Tokens->isSymbol(")")) {
    fprintf(stderr, "visitCall: expect ')' but %s\n",
      Tokens->getCurString().c_str());
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  Tokens->getNextToken(); // eat ')'
  return call_expr;
}

void Parser::check(const std::string method) {
  fprintf(stderr, "[] %s in, token: %s\n", method.c_str(), Tokens->getCurString().c_str());
}
