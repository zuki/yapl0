#include "ast.hpp"

/**
  * BlockASTメソッド
  * @param  std::unique_ptr<ConstDeclAST>
  * @return void
  */
void BlockAST::setConstant(std::unique_ptr<ConstDeclAST> constant) {
    if (!Constant) {
      Constant = std::move(constant);
    } else {
      auto dest = Constant->getNameTable();
      auto source = constant->getNameTable();
      dest.insert(dest.end(), source.begin(), source.end());
      Constant->setNameTable(dest);
    }
}

/**
  * BlockASTメソッド
  * @param  std::unique_ptr<VarDeclAST>
  */
void BlockAST::setVariable(std::unique_ptr<VarDeclAST> variable) {
    if (!Variable) {
      Variable = std::move(variable);
    } else {
      auto dest = Variable->getNameTable();
      auto source = variable->getNameTable();
      dest.insert(dest.end(), source.begin(), source.end());
      Variable->setNameTable(dest);
    }
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
  return !Constant && !Variable;
}
