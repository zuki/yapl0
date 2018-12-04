#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include "table.hpp"
#include "ast.hpp"
#include "error.hpp"

class CodeGen {
public:
  CodeGen(std::string name) :
    TheContext(), TheModule(llvm::make_unique<llvm::Module>(name, TheContext)),
    TheBuilder(TheContext) {
      setLibraries();
    }
  ~CodeGen();

  void generate(std::unique_ptr<ProgramAST> program);
  std::unique_ptr<llvm::Module> getModule() { return std::move(TheModule); }

public:
  void block(std::unique_ptr<BlockAST> block_ast, llvm::Function *func);

  void constant(std::unique_ptr<ConstDeclAST>);
  void variable(std::unique_ptr<VarDeclAST>);
  void function(std::unique_ptr<FuncDeclAST>);
  void statement(std::unique_ptr<BaseStmtAST>);
  void statementAssign(std::unique_ptr<AssignAST> stmt_ast);
  void statementIf(std::unique_ptr<IfThenAST> stmt_ast);
  void statementWhile(std::unique_ptr<WhileDoAST> stmt_ast);

  llvm::Value *condition(std::unique_ptr<CondExpAST> exp_ast);
  llvm::Value *expression(std::unique_ptr<BaseExpAST> exp_ast);
  llvm::Value *binaryExp(std::unique_ptr<BinaryExprAST> exp_ast);
  llvm::Value *callExp(std::unique_ptr<CallExprAST> exp_ast);
  llvm::Value *variableExp(std::unique_ptr<VariableAST> exp_ast);
  llvm::Value *numberExp(std::unique_ptr<NumberAST> exp_ast);

private:
  void setLibraries();
  llvm::CmpInst::Predicate token_to_inst(std::string op);

private:
  llvm::LLVMContext TheContext;
  std::unique_ptr<llvm::Module> TheModule;
  llvm::IRBuilder<> TheBuilder;
  std::unique_ptr<ProgramAST> Program;

  llvm::Function *curFunc;
  llvm::Function *writeFunc;
  llvm::Function *writelnFunc;
  Table ident_table;
};
