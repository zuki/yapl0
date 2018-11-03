#include "lexer.hpp"
#include <iostream>
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
  int line_num = 0;
  bool iscomment = false;

  ifs.open(input_filename.c_str(), std::ios::in);
  if (!ifs)
    return nullptr;

  while (ifs && getline(ifs, cur_line)) {
    char next_char;
    std::string line;
    std::unique_ptr<Token> next_token;
    int index = 0;
    int length = cur_line.length();
    bool last_char = 0;

    while(index < length) {
      next_char = cur_line.at(index++);

      //コメントアウト読み飛ばし {* コメント *}
      if (iscomment) {
        if (index < length  && next_char == '*' && cur_line.at(index++) == '}')
          iscomment = false;
        continue;
      }

      //EOF
      if (next_char == EOF) {
        token_str = EOF;
        next_token = llvm::make_unique<Token>(TOK_EOF, token_str, line_num);
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
          next_token = llvm::make_unique<Token>(TOK_CONST, token_str, line_num);
        else if (token_str == "var")
          next_token = llvm::make_unique<Token>(TOK_VAR, token_str, line_num);
        else if (token_str == "function")
          next_token = llvm::make_unique<Token>(TOK_FUNCTION, token_str, line_num);
        else if (token_str == "begin")
          next_token = llvm::make_unique<Token>(TOK_BEGIN, token_str, line_num);
        else if (token_str == "end")
          next_token = llvm::make_unique<Token>(TOK_END, token_str, line_num);
        else if (token_str == "if")
          next_token = llvm::make_unique<Token>(TOK_IF, token_str, line_num);
        else if (token_str == "then")
          next_token = llvm::make_unique<Token>(TOK_THEN, token_str, line_num);
        else if (token_str == "while")
          next_token = llvm::make_unique<Token>(TOK_WHILE, token_str, line_num);
        else if (token_str == "do")
          next_token = llvm::make_unique<Token>(TOK_DO, token_str, line_num);
        else if (token_str == "return")
          next_token = llvm::make_unique<Token>(TOK_RETURN, token_str, line_num);
        else if (token_str == "write")
          next_token = llvm::make_unique<Token>(TOK_WRITE, token_str, line_num);
        else if (token_str == "writeln")
          next_token = llvm::make_unique<Token>(TOK_WRITELN, token_str, line_num);
        else if (token_str == "odd")
          next_token = llvm::make_unique<Token>(TOK_ODD, token_str, line_num);
        else
          next_token = llvm::make_unique<Token>(TOK_IDENTIFIER, token_str, line_num);
      //数字
      } else if (isdigit(next_char)) {
        if (next_char=='0') {
          token_str += next_char;
          next_token = llvm::make_unique<Token>(TOK_DIGIT, token_str, line_num);
        } else {
          token_str += next_char;
          next_char = cur_line.at(index++);
          while (isdigit(next_char)) {
            token_str += next_char;
            next_char = cur_line.at(index++);
          }
          next_token = llvm::make_unique<Token>(TOK_DIGIT, token_str, line_num);
          index--;
        }
      // コメント {* コメント *}
      } else if (next_char == '{') {
        token_str += next_char;
        next_char = cur_line.at(index++);
        //コメントの場合
        if (next_char == '*') {
          iscomment = true;
          continue;
        } else {
          fprintf(stderr, "unclear token : {");
          return nullptr;
        }
      //それ以外(記号)
      } else if (next_char == ':') {
        token_str += next_char;
        next_char = cur_line.at(index++);
        if (next_char == '=') {
          token_str += next_char;
          next_token = llvm::make_unique<Token>(TOK_ASSIGN, token_str, line_num);
        } else {
          fprintf(stderr, "unclear token : {");
          return nullptr;
        }
      } else if (next_char == '<') {
        token_str += next_char;
        next_char = cur_line.at(index++);
        if (next_char == '>' || next_char == '=')
          token_str += next_char;
        else
          index--;
        next_token = llvm::make_unique<Token>(TOK_SYMBOL, token_str, line_num);
      } else if (next_char == '>') {
        token_str += next_char;
        next_char = cur_line.at(index++);
        if (next_char == '=')
          token_str += next_char;
        else
          index--;
        next_token = llvm::make_unique<Token>(TOK_SYMBOL, token_str, line_num);
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
          next_token = llvm::make_unique<Token>(TOK_SYMBOL, token_str, line_num);
        //解析不能字句
        } else {
          fprintf(stderr, "unclear token : %c", next_char);
          return nullptr;
        }
      }

      //Tokensに追加
      Tokens->pushToken(std::move(next_token));
      token_str.clear();
      last_char = false;
    }

    token_str.clear();
    line_num++;
  }


  //EOFの確認
  if (ifs.eof()) {
    Tokens->pushToken(
        llvm::make_unique<Token>(TOK_EOF, token_str, line_num)
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
  return *Tokens[CurIndex];
}


/**
  * インデックスを一つ増やして次のトークンに進める
  * @return 成功時：true　失敗時：false
  */
bool TokenStream::getNextToken(){
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
  * インデックスをtimes回戻す
  */
bool TokenStream::ungetToken(int times) {
  for (int i=0; i<times;i++) {
    if (CurIndex == 0)
      return false;
    else
      CurIndex--;
  }
  return true;
}


/**
  * 格納されたトークン一覧を表示する
  */
bool TokenStream::printTokens() {
  std::vector<std::unique_ptr<Token>>::iterator titer = Tokens.begin();
  while( titer != Tokens.end() ){
    fprintf(stdout,"%d:", (*titer)->getTokenType());
    if ((*titer)->getTokenType() != TOK_EOF)
      fprintf(stdout,"%s\n", (*titer)->getTokenString().c_str());
    ++titer;
  }
  return true;
}
