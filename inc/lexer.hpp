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
enum TokenType {
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
  TOK_EOF          // EOF
};

struct TokenTypeStr : public std::string {
  TokenTypeStr(TokenType t) {
    if (t == TOK_IDENTIFIER)
      assign("ident");
    else if (t == TOK_DIGIT)
      assign("number");
    else if (t == TOK_SYMBOL)
      assign("symbol");
    else if (t == TOK_EOF)
      assign("eof");
    else
      assign("keyword");
  }
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
  int Pos;       // トークンの最後の位置
  Token *Prev;

public:
  Token(){};
  Token(TokenType type, std::string string, int line, int pos, Token *prev)
    : Type(type), TokenString(string), Line(line), Pos(pos), Prev(prev){
    if (type == TOK_DIGIT)
      Number = atoi(string.c_str());
    else
      Number = 0x7fffffff;
  };
  ~Token(){};

  const TokenType &getTokenType() const { return Type; };
  const std::string &getTokenString() const { return TokenString; };
  int getNumberValue() { return Number; };
  bool setLine(int line) { Line = line; return true; }
  const int &line() const { return Line; }
  const int &pos() const { return Pos; }
  const Token *prev() const { return Prev; }
} Token;

/**
  * 切り出したToken格納用クラス
  */
class TokenStream {
private:
    std::vector<Token> Tokens;
    int CurIndex;

public:
    TokenStream(): CurIndex(0) {}
    ~TokenStream();

    bool getNextToken();
    bool pushToken(Token token) {
      Tokens.push_back(token);
      return true;
    }
    Token *getLastToken() {
      return &Tokens.back();
    }
    Token getToken();
    TokenType getCurType() { return Tokens[CurIndex].getTokenType(); }
    std::string getCurString() { return Tokens[CurIndex].getTokenString(); }
    int getCurNumVal() { return Tokens[CurIndex].getNumberValue(); }
    bool printTokens();
    int getCurIndex() { return CurIndex; }
    bool isSymbol(std::string str) { return getCurType() == TOK_SYMBOL && getCurString() == str; }
};

std::unique_ptr<TokenStream> LexicalAnalysis(std::string input_filename);

#endif  // #ifndef LEXER_HPP
