#ifndef LOG_HPP
#define LOG_HPP

#include <string>
#include <iostream>
#include <sstream>
#include "lexer.hpp"

class Log {
private:
  static const int MAXERROR = 30;
  static int error_num;

public:
  static void error(const std::string &message, Token token, bool prev=false) {
    int line, pos;
    if (prev) {
      line = token.prev()->line();
      pos  = token.prev()->pos() + 1; // posは0始まりなので+1
    } else {
      line = token.line();
      pos  = token.pos() - (int)token.getTokenString().size() + 1;
    }
    fprintf(stderr, "[% 3d:% 3d] error: %s\n", line, pos, message.c_str());
    if (error_num++ > MAXERROR) {
      fprintf(stderr, "too many errors\n");
      exit(1);
    }
  }

  static void unexpectedError(const std::string &expected, const std::string &specified, Token token, bool prev=false) {
    std::stringstream ss;
    ss << "expected '" << expected << "' but '" << specified << "'";
    error(ss.str(), token, prev);
  }

  static void unexpectedError(const std::string &specified, Token token, bool prev=false) {
    std::stringstream ss;
    ss << "unexpected '" << specified << "': deleted";
    error(ss.str(), token, prev);
  }

  static void undefinedFuncError(const std::string &name, int params, Token token, bool prev=false) {
    std::stringstream ss;
    ss << "undefined func " << name << "(" << params << ")";
    error(ss.str(), token, prev);
  }

  static void missingError(const std::string &insert, Token token, bool prev=false) {
    std::stringstream ss;
    ss << "missing '" << insert << "': inserted";
    error(ss.str(), token, prev);
  }

  static void duplicateError(const std::string &type, const std::string &name, Token token, bool prev=false) {
    std::stringstream ss;
    ss << "duplicate " << type << " " << name << ": ignored";
    error(ss.str(), token, prev);
  }

  static void skipError(const std::string &name, Token token, bool prev=false) {
    std::stringstream ss;
    ss << "delete " << name << " and skip to a new statement";
    error(ss.str(), token, prev);
  }

  static void duplicateFactorError(const std::string &name, Token token, bool prev=false) {
    std::stringstream ss;
    ss << "fact + id/num " << name << ": missing opcode";
    error(ss.str(), token, prev);
  }

  static void error(const std::string &message, bool force_exit=false) {
    fprintf(stderr, "%s\n", message.c_str());
    if (error_num++ > MAXERROR) {
      fprintf(stderr, "too many errors\n");
      exit(1);
    } else if (force_exit)
      exit(1);
  }

  static void warn(const std::string &message, Token token) {
    fprintf(stderr, "[% 3d:% 3d] warn: %s\n", token.line(), token.pos() - (int)token.getTokenString().size() + 1, message.c_str());
  }

  static void addWarn(const std::string &name, Token token) {
    std::stringstream ss;
    ss << "add " << name << " to name table temporarily";
    warn(ss.str(), token);
  }

  static void deleteWarn(const std::string &name, Token token) {
    std::stringstream ss;
    ss << "delete " << name << " from name table";
    warn(ss.str(), token);
  }

  static void token(Token token) {
      fprintf(stderr, "[% 3d:% 3d] TOKEN: %-10s (%s)\n", token.line(), token.pos() - (int)token.getTokenString().size() + 1, token.getTokenString().c_str(), TokenTypeStr(token.getTokenType()).c_str());
  }

  static int getErrorNum() {
    return error_num;
  }
};

#endif
