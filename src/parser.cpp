#include "parser.hpp"

/**
  * コンストラクタ
  */
Parser::Parser(std::string filename, bool debug = true) {
  Tokens = LexicalAnalysis(filename);
  Debug = debug;
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
  debug_check("visitProgram");
  // block
  //upLevel();
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
  debug_check("visitBlock");
  // ブロック階層を上げる
  blockIn();

  auto Block = llvm::make_unique<BlockAST>();
  while (true) {
    // std::moveするとuniq_ptrはnullになってしまうので
    // 別にフラグが必要
    bool c = false, v = false, f = false;

    auto const_decl = visitConstDecl();
    if (const_decl) {
      Block->setConstant(std::move(const_decl));
      c = true;
    }

    auto var_decl = visitVarDecl();
    if (var_decl) {
      Block->setVariable(std::move(var_decl));
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
    Block->setStatement(std::move(statement));
  } else {
    fprintf(stderr, "No statement\n");
    return nullptr;
  }

  // 未定義の定数、変数、関数にアクセス
  if (remainedTemp()) {
    fprintf(stderr, "use undefined symbol\n");
    return nullptr;
  }
  // このレベルの名前を削除
  removeSymbolsOfCurrentLevel();
  // ブロック階層を下げる
  blockOut();
  return Block;
}


// constDecl: 'const', ident, '=', number, { ',' , ident, '=', number }, ';'
/**
  * ConstDect用構文解析メソッド
  * @return 成功: std::unique_ptr<ConstDeclAST>, 失敗: nullptr
  */
std::unique_ptr<ConstDeclAST> Parser::visitConstDecl() {
  debug_check("visitConstDecl");
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
  if (findSymbol(name, CONST)) {
    fprintf(stderr, "visitConstDecl: duplicate constant %s\n", name.c_str());
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  addSymbol(name, CONST);
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
    if (findSymbol(name, CONST)) {
      fprintf(stderr, "visitConstDecl: duplicate constant %s\n", Tokens->getCurString().c_str());
      Tokens->applyTokenIndex(bkup);
      return nullptr;
    }
    addSymbol(name, CONST);
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
  debug_check("visitVarDecl");
  std::string name;

  int bkup = Tokens->getCurIndex();
  auto VarAST = llvm::make_unique<VarDeclAST>();

  if (Tokens->getCurType() != TOK_VAR)
    return nullptr;

  // eat 'var'
  Tokens->getNextToken();
  name = Tokens->getCurString();
  if (findSymbol(name, VAR)) {
    fprintf(stderr, "visitVarDecl: duplicate variable %s\n", name.c_str());
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  if (findSymbol(name, TEMP))
    deleteTemp(name);
  addSymbol(name, VAR);
  VarAST->addVariable(name);
  // eat ident
  Tokens->getNextToken();
  while(Tokens->isSymbol(",")) {
    Tokens->getNextToken(); // eat ','
    name = Tokens->getCurString();
    if (findSymbol(name, VAR)) {
      fprintf(stderr, "visitVarDecl: duplicate variable %s\n", name.c_str());
      Tokens->applyTokenIndex(bkup);
      return nullptr;
    }
    if (findSymbol(name, TEMP))
      deleteTemp(name);
    addSymbol(name, VAR);
    VarAST->addVariable(name);
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
  debug_check("visitFuncDecl");
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

  // check duplicate
  if (findSymbol(name, FUNC, true, parameters.size())) {
    fprintf(stderr, "visitFuncDecl: duplicate function %s\n", name.c_str());
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  addSymbol(name, FUNC, parameters.size());
  for (auto param : parameters) {
    if (findSymbol(param, PARAM)) {
      fprintf(stderr, "visitFuncDecl: duplicate param: %s\n", param.c_str());
      Tokens->applyTokenIndex(bkup);
      return nullptr;
    }
    addSymbol(param, PARAM);
  }

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
  * @return 成功: std::unique_ptr<BaseStmtAST>, 失敗: nullptr
  */
std::unique_ptr<BaseStmtAST> Parser::visitStatement() {
  debug_check("visitStatement");
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
  debug_check("visitAssign");
  int bkup = Tokens->getCurIndex();
  std::string name;

  name = Tokens->getCurString();
  if (!findSymbol(name, VAR) && !findSymbol(name, PARAM))
    addSymbol(name, TEMP);
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
  debug_check("visitBeginEnd");
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
  debug_check("visitIfThen");
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
  debug_check("visitWhileDo");
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
  debug_check("visitReturn");

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
  debug_check("visitWrite");

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
  debug_check("visitCondition");
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
  debug_check("visitExpression");
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
  debug_check("visitTerm");
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
  debug_check("visitFactor");
  int bkup=Tokens->getCurIndex();

  // identifier: 定義済み変数
  if (Tokens->getCurType() == TOK_IDENTIFIER) {
    std::string name = Tokens->getCurString();
    Tokens->getNextToken(); // eat ident
    if (Tokens->isSymbol("(")) {
      return visitCall(name);
    }
    if (!findSymbol(name, PARAM)
    && !findSymbol(name, VAR, false, -1)
    && !findSymbol(name, CONST, false, -1)
    && !findSymbol(name, TEMP)) {
      fprintf(stderr, "visitFactor: use undefine variable: %s\n", name.c_str());
      Tokens->applyTokenIndex(bkup);
      return nullptr;
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

// factor: ident '(' expression ')' |
/**
  * Factor(関数呼び出し)用構文解析メソッド
  * @return 成功: std::unique_ptr<BaseExpAST>, 失敗: nullptr
  */
std::unique_ptr<BaseExpAST> Parser::visitCall(const std::string &callee) {
  debug_check("visitCall");
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
  if (!findSymbol(callee, FUNC, false, call_expr->getNumOfArgs())) {
    fprintf(stderr, "visitCall: call undefined function %s\n",
      callee.c_str());
    Tokens->applyTokenIndex(bkup);
    return nullptr;
  }
  return call_expr;
}

void Parser::debug_check(const std::string method) {
  if (!Debug) return;
  //fprintf(stderr, "(%d) %s in, token: %s\n", getLevel(), method.c_str(), Tokens->getCurString().c_str());
  //fprintf(stderr, "%s in, token: %s\n", method.c_str(), Tokens->getCurString().c_str());
}

void Parser::addSymbol(std::string name, NameType type, int num) {
  int level = type == PARAM ? CurrentLevel + 1 : CurrentLevel;
  SymbolTable.emplace_back(level, type, name, num);
  if (Debug)
    fprintf(stderr, "[%d] add %s/%d(%d) at %d\n", Tokens->getLine(), name.c_str(), num > 0 ? num : 0, type, level);
}

void Parser::removeSymbolsOfCurrentLevel() {
  if (CurrentLevel == 0) return;
  for (int i = SymbolTable.size() - 1; i >= 0; i--)
    if (SymbolTable[i].level == CurrentLevel && SymbolTable.size() > 0) {
      auto e = SymbolTable.back();
      if (e.type == FUNC && CurrentLevel == e.level + 1)
        continue;
      if (Debug) {
        fprintf(stderr, "delete %s/%d(%d) at %d\n", e.name.c_str(), e.num > 0 ? e.num : 0, e.type, e.level);
      }
      SymbolTable.pop_back();
    }

}

void Parser::deleteTemp(std::string name) {
  for (int i = SymbolTable.size() - 1; i >= 0; i--) {
    auto e = SymbolTable[i];
    if (e.level == CurrentLevel && e.type == TEMP && e.name == name)
      SymbolTable.erase(SymbolTable.begin()+i);
  }
}

bool Parser::findSymbol(std::string name, NameType type, bool checkLevel, int num) {
  auto result = std::find_if(
    SymbolTable.crbegin(),
    SymbolTable.crend(),
    [&](const TableEntry &e) {
      auto cond = e.name == name && e.type == type;
      if (num != 1) cond = cond && e.num == num;
      if (checkLevel) cond = cond && e.level == CurrentLevel;
      return cond;
    });
  if (result == SymbolTable.crend())
    return false;
  return true;
}

// true if temps are remained
bool Parser::remainedTemp() {
  auto result = std::find_if(
    SymbolTable.crbegin(),
    SymbolTable.crend(),
    [&](const TableEntry &e) {
      return e.level == CurrentLevel && e.type == TEMP;
    });
  return result == SymbolTable.crend() ? false : true;
}
