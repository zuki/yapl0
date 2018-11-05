#include "ast.hpp"

/**
  * BlockASTメソッド
  * @param  std::unique_ptr<ConstDeclAST>
  * @retirm true
  */
void BlockAST::addConstant(std::unique_ptr<ConstDeclAST> constant) {
    Constants.push_back(std::move(constant));
}

/**
  * BlockASTメソッド
  * @param  std::unique_ptr<VarDeclAST>
  */
void BlockAST::addVariable(std::unique_ptr<VarDeclAST> variable) {
    Variables.push_back(std::move(variable));
}

/**
  * BlockASTメソッド
  * @param  std::unique_ptr<FuncDeclAST>ue
  */
void BlockAST::addFunction(std::unique_ptr<FuncDeclAST> function) {
    Functions.push_back(std::move(function));
}

/**
  * BlockASTメソッド
  * @retirm true
  */
bool BlockAST::empty() {
  return Constants.size() == 0 && Variables.size() == 0;
}
