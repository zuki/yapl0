#include "lexer.hpp"
#include <iostream>
#include "log.hpp"

/**
 * トークン切り出し関数
 * @param 字句解析対象ファイル名
 * @return 切り出したトークンを格納したTokenStream
 */
std::unique_ptr<TokenStream> LexicalAnalysis(std::string input_filename) {
  std::unique_ptr<TokenStream> Tokens = llvm::make_unique<TokenStream>();
  std::ifstream ifs;
  std::string cur_line;
  std::string token_str;
  int line_num = 1;
  bool iscomment = false;
  Token *prev = nullptr;

  ifs.open(input_filename.c_str(), std::ios::in);
  if (!ifs)
    return nullptr;

  while (ifs && getline(ifs, cur_line)) {
    char next_char;
    std::string line;
    Token next_token;
    int index = 0;
    int length = cur_line.length();
    bool last_char = false;

    while(index < length) {
      next_char = cur_line.at(index++);
      //fprintf(stderr, "char: %c, index: %d, length: %d\n", next_char, index, length);

      //コメントアウト読み飛ばし { コメント }
      if (iscomment) {
        if (index < length  && next_char == '}')
          iscomment = false;
        continue;
      }

      //EOF
      if (next_char == EOF) {
        token_str = EOF;
        next_token = Token(TOK_EOF, token_str, line_num, index, prev);
      } else if (isspace(next_char)){
        continue;
      //IDENTIFIER
      } else if (isalpha(next_char)) {
        token_str += next_char;
        if (index < length) {
          next_char = cur_line.at(index++);
          while (isalnum(next_char)) {
            token_str += next_char;
            if (index == length) {
              last_char = true;
              break;
            }
            next_char = cur_line.at(index++);
          }
          if (!last_char)
            index--;
        }
        if (token_str == "const")
          next_token = Token(TOK_CONST, token_str, line_num, index, prev);
        else if (token_str == "var")
          next_token = Token(TOK_VAR, token_str, line_num, index, prev);
        else if (token_str == "function")
          next_token = Token(TOK_FUNCTION, token_str, line_num, index, prev);
        else if (token_str == "begin")
          next_token = Token(TOK_BEGIN, token_str, line_num, index, prev);
        else if (token_str == "end")
          next_token = Token(TOK_END, token_str, line_num, index, prev);
        else if (token_str == "if")
          next_token = Token(TOK_IF, token_str, line_num, index, prev);
        else if (token_str == "then")
          next_token = Token(TOK_THEN, token_str, line_num, index, prev);
        else if (token_str == "while")
          next_token = Token(TOK_WHILE, token_str, line_num, index, prev);
        else if (token_str == "do")
          next_token = Token(TOK_DO, token_str, line_num, index, prev);
        else if (token_str == "return")
          next_token = Token(TOK_RETURN, token_str, line_num, index, prev);
        else if (token_str == "write")
          next_token = Token(TOK_WRITE, token_str, line_num, index, prev);
        else if (token_str == "writeln")
          next_token = Token(TOK_WRITELN, token_str, line_num, index, prev);
        else if (token_str == "odd")
          next_token = Token(TOK_ODD, token_str, line_num, index, prev);
        else
          next_token = Token(TOK_IDENTIFIER, token_str, line_num, index, prev);
      //数字
      } else if (isdigit(next_char)) {
        if (next_char=='0') {
          token_str += next_char;
          next_token = Token(TOK_DIGIT, token_str, line_num, index, prev);
        } else {
          token_str += next_char;
          if (index < length) {
            next_char = cur_line.at(index++);
            while (isdigit(next_char)) {
              token_str += next_char;
              if (index == length) {
                last_char = true;
                break;
              }
              next_char = cur_line.at(index++);
            }
            if (!last_char)
              index--;
          }
          next_token = Token(TOK_DIGIT, token_str, line_num, index, prev);
        }
      // コメント { コメント }
      } else if (next_char == '{') {
        token_str += next_char;
        next_char = cur_line.at(index++);
        iscomment = true;
        continue;
      //それ以外(記号)
      } else if (next_char == ':') {
        token_str += next_char;
        next_char = cur_line.at(index++);
        if (next_char != '=')
          Log::unexpectedError("=", {next_char}, Tokens->getToken());
        token_str += next_char;
        next_token = Token(TOK_SYMBOL, token_str, line_num, index, prev);
      } else if (next_char == '<') {
        token_str += next_char;
        next_char = cur_line.at(index++);
        if (next_char == '>' || next_char == '=')
          token_str += next_char;
        else
          index--;
        next_token = Token(TOK_SYMBOL, token_str, line_num, index, prev);
      } else if (next_char == '>') {
        token_str += next_char;
        next_char = cur_line.at(index++);
        if (next_char == '=')
          token_str += next_char;
        else
          index--;
        next_token = Token(TOK_SYMBOL, token_str, line_num, index, prev);
      } else {
        if (next_char == '*' ||
            next_char == '+' ||
            next_char == '-' ||
            next_char == '/' ||
            next_char == '=' ||
            next_char == ';' ||
            next_char == ',' ||
            next_char == '.' ||
            next_char == '(' ||
            next_char == ')') {
          token_str += next_char;
          next_token = Token(TOK_SYMBOL, token_str, line_num, index, prev);
        //解析不能字句
        } else {
          Log::unexpectedError({next_char}, Tokens->getToken());
        }
      }

      //Tokensに追加
      Tokens->pushToken(next_token);
      prev = Tokens->getLastToken();
      token_str.clear();
    }

    token_str.clear();
    line_num++;
    last_char = false;
  }


  //EOFの確認
  if (ifs.eof()) {
    Tokens->pushToken(
        Token(TOK_EOF, token_str, line_num, -1, prev)
        );
  }

  //クローズ
  ifs.close();
  return Tokens;
}



/**
  * デストラクタ
  */
TokenStream::~TokenStream() {
  Tokens.clear();
}


/**
  * トークン取得
  * @return CureIndex番目のToken
  */
Token TokenStream::getToken() {
  return Tokens[CurIndex];
}

/**
  * インデックスを一つ増やして次のトークンに進める
  * @return 成功時：true　失敗時：false
  */
bool TokenStream::getNextToken(){
  //fprintf(stderr, "eat %s\n", Tokens[CurIndex]->getTokenString().c_str());
  int size = Tokens.size();
  if(--size == CurIndex){
    return false;
  } else if (CurIndex < size) {
    CurIndex++;
    return true;
  } else {
    return false;
  }
}


/**
  * 格納されたトークン一覧を表示する
  */
bool TokenStream::printTokens() {
  for(size_t i = 0; i < Tokens.size(); i++) {
    Log::token(Tokens[i]);
  }
  return true;
}
