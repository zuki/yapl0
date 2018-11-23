#pragma once

#include <iostream>
#include <string>

static void undefinedError(const std::string &ident) {
  std::cerr << "undefined error: " << ident << " is undefined" << std::endl;
  exit(1);
}

static void error(const std::string &msg) {
  std::cerr << "error: " << msg << std::endl;
  exit(1);
}
