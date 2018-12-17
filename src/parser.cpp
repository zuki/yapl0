#include "parser.hpp"
#include "log.hpp"
#include "table.hpp"
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
bool Parser::parse() {
  std::stringstream ss;
  if (!Tokens) {
    Log::error("error at lexer: could not make Tokens");
    return false;
  }
  bool result = parseProgram();
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
bool Parser::parseProgram() {
  bool result = true;

  // block
  sym_table.blockIn();
  std::unique_ptr<BlockAST> Block = parseBlock();
  if (!Block || Block->empty()) {
    Log::error("error at parseBlock", false);
    result = false;
  } else {
    TheProgramAST =  llvm::make_unique<ProgramAST>(std::move(Block));
  }

  // eat '.'
  if(!Tokens->isSymbol(".")) {
    Log::error("program done without '.': insert", Tokens->getToken());
  }

  // 未定義の定数、変数、関数にアクセス
  if (sym_table.remainedTemp()) {
    Log::error("remain undefined symbols", false);
    sym_table.dumpTempNames();
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
std::unique_ptr<BlockAST> Parser::parseBlock() {
  auto Block = llvm::make_unique<BlockAST>();
  while (true) {
    // std::moveするとuniq_ptrはnullになってしまうので
    // 別にフラグが必要
    //bool c = false, v = false, f = false;
    if (Tokens->getCurType() == TOK_CONST) {
      Tokens->getNextToken(); // eat 'const'
      auto const_decl = parseConst();
      if (const_decl)
        Block->setConstant(std::move(const_decl));
    } else if (Tokens->getCurType() == TOK_VAR) {
      Tokens->getNextToken(); // eat 'var'
      auto var_decl = parseVar();
      if (var_decl)
        Block->setVariable(std::move(var_decl));
    } else if (Tokens->getCurType() == TOK_FUNCTION) {
      Tokens->getNextToken(); // eat 'function'
      auto func_decl = parseFunction();
      if (func_decl)
        Block->addFunction(std::move(func_decl));
    } else {
      break;
    }
  }
  auto statement = parseStatement();
  if (statement) {
    Block->setStatement(std::move(statement));
  } else {
    Log::error("No statement", false);
    Block = nullptr;
  }

  // ブロック階層を下げる
  sym_table.blockOut();
  return Block;
}


// constDecl: 'const', ident, '=', number, { ',' , ident, '=', number }, ';'
/**
  * ConstDect用構文解析メソッド
  * @return 成功: std::unique_ptr<ConstDeclAST>, 失敗: nullptr
  */
std::unique_ptr<ConstDeclAST> Parser::parseConst() {
  std::string name;

  auto ConstAST = llvm::make_unique<ConstDeclAST>();

  while(true) {
    if (Tokens->getCurType() != TOK_IDENTIFIER) {
      Log::error("missing const name", Tokens->getToken());
    } else {
      name = Tokens->getCurString();
      Tokens->getNextToken();   // eat ident
      checkGet("=");
      if (sym_table.findSymbol(name, CONST))
        Log::duplicateError("constant", name, Tokens->getToken());
      else {
        if (sym_table.findTemp(name)) {
          sym_table.deleteTemp(name);
          Log::deleteWarn(name, Tokens->getToken());
        }
        sym_table.addSymbol(name, CONST);
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
std::unique_ptr<VarDeclAST> Parser::parseVar() {
  std::string name;

  auto VarAST = llvm::make_unique<VarDeclAST>();

  while(true) {
    if (Tokens->getCurType() != TOK_IDENTIFIER) {
      Log::error("missing var name", Tokens->getToken());
    } else {
      name = Tokens->getCurString();
      if (sym_table.findSymbol(name, VAR)) {
        Log::duplicateError("var", name, Tokens->getToken());
      } else {
        if (sym_table.findTemp(name)) {
          sym_table.deleteTemp(name);
          Log::deleteWarn(name, Tokens->getToken());
        }
        sym_table.addSymbol(name, VAR);
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
std::unique_ptr<FuncDeclAST> Parser::parseFunction() {
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
      if (sym_table.findSymbol(param, PARAM)) {
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
  if (sym_table.findSymbol(name, FUNC, true, parameters.size())) {
    Log::duplicateError("func", name.c_str(), temp);
    return nullptr;
  }
  sym_table.addSymbol(name, FUNC, parameters.size());
  // ここからブロックレベルを上げる
  sym_table.blockIn();
  for (auto param : parameters)
    sym_table.addSymbol(param, PARAM);

  auto block = parseBlock();
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
std::unique_ptr<BaseStmtAST> Parser::parseStatement() {
  std::unique_ptr<BaseStmtAST> statement;

  switch (Tokens->getCurType()) {
    case TOK_IDENTIFIER:
      statement = parseAssign();
      break;
    case TOK_BEGIN:
      statement = parseBeginEnd();
      break;
    case TOK_IF:
      statement = parseIfThen();
      break;
    case TOK_WHILE:
      statement = parseWhileDo();
      break;
    case TOK_RETURN:
      statement = parseReturn();
      break;
    case TOK_WRITE:
      statement = parseWrite();
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
std::unique_ptr<BaseStmtAST> Parser::parseAssign() {
  std::string name;

  name = Tokens->getCurString();
  if (sym_table.findSymbol(name, FUNC, false, -1)) {
    Log::error("assign lhs is not var/par", Tokens->getToken());
  } else if (!sym_table.findSymbol(name, VAR) && !sym_table.findSymbol(name, PARAM)) {
    sym_table.addTemp(name);
    Log::addWarn(name, Tokens->getToken());
  }

  Tokens->getNextToken(); // eat ident
  checkGet(":=");
  auto rhs = parseExpression(nullptr);
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
std::unique_ptr<BaseStmtAST> Parser::parseBeginEnd() {
  std::unique_ptr<BaseStmtAST> statement;
  std::vector<std::unique_ptr<BaseStmtAST>> statements;

  Tokens->getNextToken(); // eat 'begin'
  while(true) {
    statement = parseStatement();
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
std::unique_ptr<BaseStmtAST> Parser::parseIfThen() {
  Tokens->getNextToken(); // eat 'if'
  auto temp = Tokens->getToken();
  auto condition = parseCondition();
  if (!condition) {
    Log::error("Couldn't get condition of if condition", temp);
    return nullptr;
  }
  checkGet("then");
  auto temp2 = Tokens->getToken();
  auto statement = parseStatement();
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
std::unique_ptr<BaseStmtAST> Parser::parseWhileDo() {
  Tokens->getNextToken(); // eat 'while'
  auto temp = Tokens->getToken();
  auto condition = parseCondition();
  if (!condition) {
    Log::error("Couldn't get condition of while condition", temp);
    return nullptr;
  }
  checkGet("do");
  auto temp2 = Tokens->getToken();
  auto statement = parseStatement();
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
std::unique_ptr<BaseStmtAST> Parser::parseReturn() {
  Tokens->getNextToken(); // eat 'return'
  auto temp = Tokens->getToken();
  auto expression = parseExpression(nullptr);
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
std::unique_ptr<BaseStmtAST> Parser::parseWrite() {
  Tokens->getNextToken(); // eat 'write'
  auto temp = Tokens->getToken();
  auto expression = parseExpression(nullptr);
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
std::unique_ptr<BaseExpAST> Parser::parseCondition() {
  if (Tokens->getCurType() == TOK_ODD) {
    Tokens->getNextToken(); // eat 'odd'
    auto temp = Tokens->getToken();
    auto rhs = parseExpression(nullptr);
    if (!rhs) {
      Log::error("Couldn't get odd expr of condition", temp);
      return nullptr;
    }
    return llvm::make_unique<CondExpAST>("odd", nullptr, std::move(rhs));
  }

  auto temp2 = Tokens->getToken();
  auto lhs = parseExpression(nullptr);
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
  auto rhs = parseExpression(nullptr);
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
std::unique_ptr<BaseExpAST> Parser::parseExpression(std::unique_ptr<BaseExpAST> lhs) {
  std::string prefix = "";
  std::string op;

  if (Tokens->isSymbol("+") || Tokens->isSymbol("-")) {
    prefix = Tokens->getCurString();
    Tokens->getNextToken();  // eat prefix
  }

  auto temp = Tokens->getToken();
  if (!lhs)
    lhs = parseTerm(nullptr);
  if (!lhs) {
    Log::error("Couldn't get lhs expr of expression", temp);
    return nullptr;
  }

  if (Tokens->isSymbol("+") || Tokens->isSymbol("-")) {
    op = Tokens->getCurString();
    Tokens->getNextToken(); // eat '+' of '-'
    auto temp2 = Tokens->getToken();
    auto rhs = parseTerm(nullptr);
    if (rhs) {
      return parseExpression(llvm::make_unique<BinaryExprAST>(op, std::move(lhs), std::move(rhs), prefix));
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
std::unique_ptr<BaseExpAST> Parser::parseTerm(std::unique_ptr<BaseExpAST> lhs) {
  std::string op;

  auto temp = Tokens->getToken();
  if (!lhs)
    lhs = parseFactor();
  if (!lhs) {
    Log::error("Couldn't get lhs expr of term", temp);
    return nullptr;
  }

  if (Tokens->isSymbol("*") || Tokens->isSymbol("/")) {
    op = Tokens->getCurString();
    Tokens->getNextToken(); // eat '*' or '/'
    auto temp2 = Tokens->getToken();
    auto rhs = parseFactor();
    if (rhs) {
      return parseTerm(llvm::make_unique<BinaryExprAST>(op, std::move(lhs), std::move(rhs)));
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
std::unique_ptr<BaseExpAST> Parser::parseFactor() {
  // identifier: 定義済み変数
  std::unique_ptr<BaseExpAST> baseAST;
  if (Tokens->getCurType() == TOK_IDENTIFIER) {
    std::string name = Tokens->getCurString();
    auto temp = Tokens->getToken();
    Tokens->getNextToken(); // eat ident
    if (sym_table.findSymbol(name, FUNC, false, -1)) {
      baseAST = parseCall(name, temp);
    } else {
      if (!sym_table.findSymbol(name, PARAM)
      && !sym_table.findSymbol(name, VAR, false, -1)
      && !sym_table.findSymbol(name, CONST, false, -1)
      && !sym_table.findTemp(name)) {
        sym_table.addTemp(name);
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
    auto expr = parseExpression(nullptr);
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
std::unique_ptr<BaseExpAST> Parser::parseCall(const std::string &callee, Token token) {
  auto call_expr = llvm::make_unique<CallExprAST>(callee);
  Tokens->getNextToken(); // eat '('
  auto arg = parseExpression(nullptr);
  if (arg) {
    while (true) {
      call_expr->addArg(std::move(arg));
      if (!Tokens->isSymbol(","))
        break;
      Tokens->getNextToken(); // eat ','
      arg = parseExpression(nullptr);
    }
  }
  checkGet(")");

  if (!sym_table.findSymbol(callee, FUNC, false, call_expr->getNumOfArgs())) {
    Log::undefinedFuncError(callee, call_expr->getNumOfArgs(), token);
    return nullptr;
  }
  return call_expr;
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
  fprintf(stderr, "%s: name: %s, type: %s\n", caller.c_str(), Tokens->getCurString().c_str(), TokenTypeStr(Tokens->getCurType()).c_str());
}

bool Parser::isStmtBeginKey(const std::string &name) {
  std::vector<std::string> words = {
    "begin", "if", "while", "return", "write", "writeln"
  };
  auto result = std::find(words.begin(), words.end(), name);
  return result != words.end();
}
