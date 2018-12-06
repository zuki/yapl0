#include "parser.hpp"
#include "log.hpp"
#include <iostream>
#include <sstream>

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
  std::stringstream ss;
  if (!Tokens) {
    Log::error("error at lexer: could not make Tokens");
    return false;
  }
  bool result = visitProgram();
  int num = Log::getErrorNum();
  if (num >= 1) {
    ss << num << " error";
    if (num > 1)
      ss << "s";
    Log::error(ss.str());
  }

  return result && (num < MINERROR);
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
  bool result = true;

  // block
  blockIn();
  std::unique_ptr<BlockAST> Block = visitBlock();
  if (!Block || Block->empty()) {
    Log::error("error at visitBlock", false);
    result = false;
  } else {
    TheProgramAST =  llvm::make_unique<ProgramAST>(std::move(Block));
  }

  // eat '.'
  if(!Tokens->isSymbol(".")) {
    Log::error("program done without '.': insert", Tokens->getToken());
  }

  // 未定義の定数、変数、関数にアクセス
  if (remainedTemp()) {
    Log::error("remain undefined symbols", false);
    dumpTempNames();
    result = false;
  }

  return result;
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
    // std::moveするとuniq_ptrはnullになってしまうので
    // 別にフラグが必要
    //bool c = false, v = false, f = false;
    if (Tokens->getCurType() == TOK_CONST) {
      Tokens->getNextToken(); // eat 'const'
      auto const_decl = visitConstDecl();
      if (const_decl)
        Block->setConstant(std::move(const_decl));
    } else if (Tokens->getCurType() == TOK_VAR) {
      Tokens->getNextToken(); // eat 'var'
      auto var_decl = visitVarDecl();
      if (var_decl)
        Block->setVariable(std::move(var_decl));
    } else if (Tokens->getCurType() == TOK_FUNCTION) {
      Tokens->getNextToken(); // eat 'function'
      auto func_decl = visitFuncDecl();
      if (func_decl)
        Block->addFunction(std::move(func_decl));
    } else {
      break;
    }
  }
  auto statement = visitStatement();
  if (statement) {
    Block->setStatement(std::move(statement));
  } else {
    Log::error("No statement", false);
    Block = nullptr;
  }

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
  std::string name;
  std::size_t pos;

  auto ConstAST = llvm::make_unique<ConstDeclAST>();

  while(true) {
    if (Tokens->getCurType() != TOK_IDENTIFIER) {
      Log::error("missing const name", Tokens->getToken());
    } else {
      name = Tokens->getCurString();
      Tokens->getNextToken();   // eat ident
      checkGet("=");
      if (findSymbol(name, CONST) > -1)
        Log::duplicateError("constant", name, Tokens->getToken());
      else {
        if ((pos = findTemp(name)) > -1) {
          deleteTemp(pos);
          Log::deleteWarn(name, Tokens->getToken());
        }
        addSymbol(name, CONST);
      }
      if (Tokens->getCurType() == TOK_DIGIT)
        ConstAST->addConstant(name, Tokens->getCurNumVal());
      else
        Log::error("assigned not number", Tokens->getToken());
      Tokens->getNextToken(); // eat number
    }
    if (!Tokens->isSymbol(",")) {
      if (Tokens->getCurType() == TOK_IDENTIFIER) {
        Log::missingError(",", Tokens->getToken());
        continue;
      } else {
        break;
      }
    }
    Tokens->getNextToken(); // eat ','
  }
  checkGet(";");

  return ConstAST;
}

// varDecl: 'var', ident, { ',', ident }, ';'
/**
  * VarDecl用構文解析メソッド
  * @return 成功: std::unique_ptr<VarDeclAST>, 失敗: nullptr
  */
std::unique_ptr<VarDeclAST> Parser::visitVarDecl() {
  std::string name;
  int pos;

  auto VarAST = llvm::make_unique<VarDeclAST>();

  while(true) {
    if (Tokens->getCurType() != TOK_IDENTIFIER) {
      Log::error("missing var name", Tokens->getToken());
    } else {
      name = Tokens->getCurString();
      if (findSymbol(name, VAR) > -1) {
        Log::duplicateError("var", name, Tokens->getToken());
      } else {
        if ((pos = findTemp(name)) > -1) {
          deleteTemp(pos);
          Log::deleteWarn(name, Tokens->getToken());
        }
        addSymbol(name, VAR);
        VarAST->addVariable(name);
      }
      Tokens->getNextToken();   // eat ident
    }
    if (!Tokens->isSymbol(",")) {
      if (Tokens->getCurType() == TOK_IDENTIFIER) {
        Log::missingError(",", Tokens->getToken());
        continue;
      } else {
        break;
      }
    }
    Tokens->getNextToken(); // eat ','
  }
  checkGet(";");

  return VarAST;
}

// funcDecl: ''function', ident, '(', [ ident, { ',', ident } ], ')', block, ';'
/**
  * FuncDecl用構文解析メソッド
  * @return 成功: std::unique_ptr<FuncDeclAST>, 失敗: nullptr
  */
std::unique_ptr<FuncDeclAST> Parser::visitFuncDecl() {
  std::string name, param;
  std::vector<std::string> parameters;

  if (Tokens->getCurType() != TOK_IDENTIFIER) {
    Log::error("missing function name", Tokens->getToken());
    return nullptr;
  }

  name = Tokens->getCurString();
  auto temp = Tokens->getToken();
  Tokens->getNextToken(); // eat ident
  checkGet("(");
  while(true){
    if (Tokens->getCurType() == TOK_IDENTIFIER) {
      param = Tokens->getCurString();
      if (findSymbol(param, PARAM) > -1) {
          Log::duplicateError("param", param.c_str(), Tokens->getToken());
      } else {
          parameters.push_back(param);
      }
      Tokens->getNextToken(); // eat ident
    } else {
      break; // 最初に識別子が来なければparamtersは終了
    }
    if (!Tokens->isSymbol(",")) {
      if (Tokens->getCurType() == TOK_IDENTIFIER) {
        Log::missingError(",", Tokens->getToken());
        continue;
      } else {
        break;
      }
    }
    Tokens->getNextToken(); // eat ','
  }
  checkGet(")");
  if (Tokens->isSymbol(";")) {
    Log::unexpectedError(";", Tokens->getToken());
    Tokens->getNextToken(); // eat ';'
  }
  // check duplication of function
  if (findSymbol(name, FUNC, true, parameters.size()) > -1) {
    Log::duplicateError("func", name.c_str(), temp);
    return nullptr;
  }
  addSymbol(name, FUNC, parameters.size());
  // ここからブロックレベルを上げる
  blockIn();
  for (auto param : parameters)
    addSymbol(param, PARAM);

  auto block = visitBlock();
  if (!block) {
    Log::error("error in function block");
    return nullptr;
  }
  checkGet(";");
  return llvm::make_unique<FuncDeclAST>(name, parameters, std::move(block));
}

// statment
/**
  * Statement用構文解析メソッド
  * @return 成功: std::unique_ptr<BaseStmtAST>, 失敗: nullptr
  */
std::unique_ptr<BaseStmtAST> Parser::visitStatement() {
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
        Log::skipError(Tokens->getCurString().c_str(), Tokens->getToken());
        Tokens->getNextToken();
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
  std::string name;

  name = Tokens->getCurString();
  if (findSymbol(name, FUNC, false, -1) > -1) {
    Log::error("assign lhs is not var/par", Tokens->getToken());
  } else if (findSymbol(name, VAR) == -1 && findSymbol(name, PARAM) == -1) {
    addSymbol(name, TEMP);
    Log::addWarn(name, Tokens->getToken());
  }

  Tokens->getNextToken(); // eat ident
  checkGet(":=");
  auto rhs = visitExpression(nullptr);
  if (!rhs) {
    Log::error("Couldn't get rhs-expr of assignment", Tokens->getToken());
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
  std::unique_ptr<BaseStmtAST> statement;
  std::vector<std::unique_ptr<BaseStmtAST>> statements;

  Tokens->getNextToken(); // eat 'begin'
  while(true) {
    statement = visitStatement();
    if (!statement) {
      return nullptr;
    }
    statements.push_back(std::move(statement));
    while(true) {
      if (Tokens->isSymbol(";")) {
        Tokens->getNextToken(); // eat ';'
        break;
      }
      if (Tokens->getCurType() == TOK_END) {
        Tokens->getNextToken(); // eat ';'
        return llvm::make_unique<BeginEndAST>(std::move(statements));
      }
      if (isStmtBeginKey(Tokens->getCurString())) {
        Log::missingError("';'", Tokens->getToken(), true);
        break;
      }
      Log::skipError(Tokens->getCurString(), Tokens->getToken());
      Tokens->getNextToken();
    }
  }
}

// 'if' condition 'then' statement
/**
  * BeginEnd用構文解析メソッド
  * @return 成功: std::unique_ptr<BaseStmtAST>, 失敗: nullptr
  */
std::unique_ptr<BaseStmtAST> Parser::visitIfThen() {
  Tokens->getNextToken(); // eat 'if'
  auto temp = Tokens->getToken();
  auto condition = visitCondition();
  if (!condition) {
    Log::error("Couldn't get condition of if condition", temp);
    return nullptr;
  }
  checkGet("then");
  auto temp2 = Tokens->getToken();
  auto statement = visitStatement();
  if (!statement) {
    Log::error("Couldn't get statement of if then", temp2);
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
  Tokens->getNextToken(); // eat 'while'
  auto temp = Tokens->getToken();
  auto condition = visitCondition();
  if (!condition) {
    Log::error("Couldn't get condition of while condition", temp);
    return nullptr;
  }
  checkGet("do");
  auto temp2 = Tokens->getToken();
  auto statement = visitStatement();
  if (!statement) {
    Log::error("Couldn't get statement of while do", temp2);
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
  Tokens->getNextToken(); // eat 'return'
  auto temp = Tokens->getToken();
  auto expression = visitExpression(nullptr);
  if (!expression) {
    Log::error("Couldn't get expr of return", temp);
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
  Tokens->getNextToken(); // eat 'write'
  auto temp = Tokens->getToken();
  auto expression = visitExpression(nullptr);
  if (!expression) {
    Log::error("Couldn't get expr of write", temp);
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
  if (Tokens->getCurType() == TOK_ODD) {
    Tokens->getNextToken(); // eat 'odd'
    auto temp = Tokens->getToken();
    auto rhs = visitExpression(nullptr);
    if (!rhs) {
      Log::error("Couldn't get odd expr of condition", temp);
      return nullptr;
    }
    return llvm::make_unique<CondExpAST>("odd", nullptr, std::move(rhs));
  }

  auto temp2 = Tokens->getToken();
  auto lhs = visitExpression(nullptr);
  if (!lhs) {
    Log::error("Couldn't get lhs expr of condition", temp2);
    return nullptr;
  }
  std::string op = Tokens->getCurString();
  if (!(op == "=" || op == "<>" || op == "<" ||
        op == ">" || op == "<=" || op == ">=")) {
    Log::unexpectedError(Tokens->getCurString().c_str(), Tokens->getToken());
    return nullptr;
  }
  Tokens->getNextToken(); // eat Symbol
  auto temp3 = Tokens->getToken();
  auto rhs = visitExpression(nullptr);
  if (!rhs) {
    Log::error("Couldn't get lhs expr of condition", temp3);
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
  std::string prefix = "";
  std::string op;

  if (Tokens->isSymbol("+") || Tokens->isSymbol("-")) {
    prefix = Tokens->getCurString();
    Tokens->getNextToken();  // eat prefix
  }

  auto temp = Tokens->getToken();
  if (!lhs)
    lhs = visitTerm(nullptr);
  if (!lhs) {
    Log::error("Couldn't get lhs expr of expression", temp);
    return nullptr;
  }

  if (Tokens->isSymbol("+") || Tokens->isSymbol("-")) {
    op = Tokens->getCurString();
    Tokens->getNextToken(); // eat '+' of '-'
    auto temp2 = Tokens->getToken();
    auto rhs = visitTerm(nullptr);
    if (rhs) {
      return visitExpression(llvm::make_unique<BinaryExprAST>(op, std::move(lhs), std::move(rhs), prefix));
    } else {
      Log::error("Couldn't get rhs expr of expression", temp2);
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
  std::string op;

  auto temp = Tokens->getToken();
  if (!lhs)
    lhs = visitFactor();
  if (!lhs) {
    Log::error("Couldn't get lhs expr of term", temp);
    return nullptr;
  }

  if (Tokens->isSymbol("*") || Tokens->isSymbol("/")) {
    op = Tokens->getCurString();
    Tokens->getNextToken(); // eat '*' or '/'
    auto temp2 = Tokens->getToken();
    auto rhs = visitFactor();
    if (rhs) {
      return visitTerm(llvm::make_unique<BinaryExprAST>(op, std::move(lhs), std::move(rhs)));
    } else {
      Log::error("Couldn't get rhs expr of term", temp2);
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
  // identifier: 定義済み変数
  std::unique_ptr<BaseExpAST> baseAST;
  if (Tokens->getCurType() == TOK_IDENTIFIER) {
    std::string name = Tokens->getCurString();
    auto temp = Tokens->getToken();
    Tokens->getNextToken(); // eat ident
    if (findSymbol(name, FUNC, false, -1) != -1) {
      baseAST = visitCall(name, temp);
    } else {
      if (findSymbol(name, PARAM) == -1
      && findSymbol(name, VAR, false, -1) == -1
      && findSymbol(name, CONST, false, -1) == -1
      && findTemp(name) == -1) {
        addTemp(name);
        Log::addWarn(name, Tokens->getToken());
      }
      baseAST = llvm::make_unique<VariableAST>(name);
    }
  } else if (Tokens->getCurType() == TOK_DIGIT) {
    int val=Tokens->getCurNumVal();
    Tokens->getNextToken(); // eat digit
    baseAST = llvm::make_unique<NumberAST>(val);
  } else if (Tokens->isSymbol("(")) {
    Tokens->getNextToken(); // eat '('
    auto temp = Tokens->getToken();
    auto expr = visitExpression(nullptr);
    if (!expr) {
      Log::error("Couldn't get expr of paren", temp);
      return nullptr;
    }
    checkGet(")");
    baseAST = std::move(expr);
  } else {
    baseAST = nullptr;
  }
  if (Tokens->getCurType() == TOK_IDENTIFIER || Tokens->getCurType() == TOK_DIGIT) {
    Log::duplicateFactorError(Tokens->getCurString(), Tokens->getToken());
  } if (Tokens->isSymbol("(")) {
    Log::error("factor + '(': missing opcode", Tokens->getToken());
  }
  return baseAST;
}

// factor: ident '(' expression ')' |
/**
  * Factor(関数呼び出し)用構文解析メソッド
  * @return 成功: std::unique_ptr<BaseExpAST>, 失敗: nullptr
  */
std::unique_ptr<BaseExpAST> Parser::visitCall(const std::string &callee, Token token) {
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
  checkGet(")");

  if (findSymbol(callee, FUNC, false, call_expr->getNumOfArgs())  == -1) {
    Log::undefinedFuncError(callee, call_expr->getNumOfArgs(), token);
    return nullptr;
  }
  return call_expr;
}

void Parser::addSymbol(std::string name, NameType type, int num) {
  SymbolTable.emplace_back(CurrentLevel, type, name, num);
}

void Parser::addTemp(std::string name) {
  TempNames.push_back(name);
}

void Parser::deleteTemp(int pos) {
  TempNames.erase(TempNames.begin()+pos);
}

int Parser::findTemp(const std::string &name) {
  auto it = std::find(TempNames.cbegin(), TempNames.cend(), name);
  if (it == TempNames.cend())
    return -1;
  return (int)std::distance(TempNames.cbegin(), it);
}

// SymbolTableから条件にあうTableEntryを探す
//   見つかった場合は、その位置 (iterator)
//   見つからなかった場合は -1
int Parser::findSymbol(const std::string &name, const NameType &type, const bool &checkLevel, const int &num) {
  auto result = std::find_if(
    SymbolTable.crbegin(),
    SymbolTable.crend(),
    [&](const TableEntry &e) {
      auto cond = e.name == name && e.type == type;
      if (num != -1) cond = cond && e.num == num;
      if (checkLevel) cond = cond && e.level == CurrentLevel;
      return cond;
    });
  if (result == SymbolTable.crend())
    return -1;
  return -(result - SymbolTable.crend() + 1);
}

// true if temps are remained
bool Parser::remainedTemp() {
  return TempNames.size() > 0;
}

bool Parser::isKeyWord(std::string &name) {
  std::vector<std::string> keywords = {
    "begin", "end", "if", "then", "while", "do", "return",
    "function", "var", "const", "odd", "write", "writeln"
  };
  auto result = std::find(keywords.begin(), keywords.end(), name);
  if (result == keywords.end())
    return false;
  return true;
}

bool Parser::isSymbol(std::string &name) {
  std::vector<std::string> symbols = {
    "<", "<>", "<=", ">", ">=", "*", "/",
    "+", "-", ";", ",", ".", "=", "(", ")", ":="
  };
  auto result = std::find(symbols.begin(), symbols.end(), name);
  if (result == symbols.end())
    return false;
  return true;
}

bool Parser::isKeyWordType(int type) {
  return TOK_CONST <= type && type <= TOK_ODD;
}

void Parser::checkGet(std::string symbol) {
  //check("in checkGet");
  //fprintf(stderr, "symbol: %s\n", symbol.c_str());
  if (Tokens->getCurString() == symbol) {
    Tokens->getNextToken();
    return;
  }
  if ((isKeyWord(symbol) && isKeyWordType(Tokens->getCurType())) ||
        (isSymbol(symbol) && (Tokens->getCurType() == TOK_SYMBOL))) {
    Log::unexpectedError(Tokens->getCurString(), Tokens->getToken());
    Log::missingError(symbol, Tokens->getToken());
    Tokens->getNextToken();
  } else {
    auto token = Tokens->getToken();
    Log::missingError(symbol, Tokens->getToken(), true);
  }
}

void Parser::check(const std::string &caller) {
  std::string type_name[] = {
    "ident", "number", "symbol", "kw", "kw", "kw", "kw", "kw", "kw",
    "kw", "kw", "kw", "kw", "kw", "kw", "kw", "eof"
  };
  fprintf(stderr, "%s: name: %s, type: %s\n", caller.c_str(), Tokens->getCurString().c_str(), type_name[Tokens->getCurType()].c_str());
}

bool Parser::isStmtBeginKey(const std::string &name) {
  std::vector<std::string> words = {
    "begin", "if", "while", "return", "write", "writeln"
  };
  auto result = std::find(words.begin(), words.end(), name);
  if (result == words.end())
    return false;
  return true;
}

void Parser::dumpNameTable() {
  std::string type_name[] = { "CONST", "VAR", "PARAM", "FUNC", "TEMP" };
  for (const TableEntry &e : SymbolTable)
    fprintf(stderr, "[%d] %-10s %s\n", e.level, e.name.c_str(), type_name[e.type].c_str());
}

void Parser::dumpTempNames() {
  fprintf(stderr, "remain symbols:");
  for (const std::string &name : TempNames)
    fprintf(stderr, " %s", name.c_str());
  fprintf(stderr, "\n");
}
