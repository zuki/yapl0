#ifndef LEXER_HPP
#define LEXER_HPP

#include "llvm/ADT/STLExtras.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <list>
#include <string>
#include <vector>


/**
  * トークン種別
  */
enum TokenType{
  TOK_IDENTIFIER,  // 識別子
  TOK_DIGIT,       // 数字
  TOK_SYMBOL,      // 記号
  TOK_CONST,       // Keyword: const
  TOK_VAR,         // Keyword: var
  TOK_FUNCTION,    // Keyword: function
  TOK_BEGIN,       // Keyword: begin
  TOK_END,         // Keyword: end
  TOK_IF,          // Keyword: if
  TOK_THEN,        // Keyword: then
  TOK_WHILE,       // Keyword: while
  TOK_DO,          // Keyword: do
  TOK_RETURN,      // Keyword: return
  TOK_WRITE,       // Keyword: write
  TOK_WRITELN,     // Keyword: writeln
  TOK_ODD,         // Keyword: odd
  TOK_ASSIGN,      // Symbol: :=
  TOK_EOF          // EOF
};

/**
  *個別トークン格納クラス
  */
typedef class Token {
public:

private:
  TokenType Type;
  std::string TokenString;
  int Number;
  int Line;

public:
  Token(TokenType type, std::string string, int line)
    : Type(type), TokenString(string), Line(line){
    if (type == TOK_DIGIT)
      Number = atoi(string.c_str());
    else
      Number = 0x7fffffff;
  };
  ~Token(){};

  TokenType getTokenType() { return Type; };
  std::string getTokenString() { return TokenString; };
  int getNumberValue() { return Number; };
  bool setLine(int line) { Line = line; return true; }
  int getLine() { return Line; }

} Token;

/**
  * 切り出したToken格納用クラス
  */
class TokenStream {
private:
    std::vector<std::unique_ptr<Token>> Tokens;
    int CurIndex;

public:
    TokenStream(): CurIndex(0) {}
    ~TokenStream();


    bool ungetToken(int Times=1);
    bool getNextToken();
    bool pushToken(std::unique_ptr<Token> token) {
      Tokens.push_back(std::move(token));
      return true;
    }
    Token getToken();
    TokenType getCurType() { return Tokens[CurIndex]->getTokenType(); }
    std::string getCurString() { return Tokens[CurIndex]->getTokenString(); }
    int getCurNumVal() { return Tokens[CurIndex]->getNumberValue(); }
    bool printTokens();
    int getCurIndex() { return CurIndex; }
    bool applyTokenIndex(int index) { CurIndex=index;return true; }
    bool isSymbol(std::string str) { return getCurType() == TOK_SYMBOL && getCurString() == str; }
    int getLine() { return Tokens[CurIndex]->getLine(); }
};

std::unique_ptr<TokenStream> LexicalAnalysis(std::string input_filename);

#endif  // #ifndef LEXER_HPP
