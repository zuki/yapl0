#include "llvm/IR/LegacyPassManager.h"
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/ValueSymbolTable.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include "llvm/LinkAllPasses.h"
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"
#include <string>
#include "ast.hpp"
#include "table.hpp"
#include "codegen.hpp"
#include "log.hpp"

CodeGen::~CodeGen(){}

void CodeGen::generate(std::unique_ptr<ProgramAST> program) {
  Program = std::move(program);
  auto *funcType = llvm::FunctionType::get(TheBuilder.getInt64Ty(), false);
  auto *mainFunc = llvm::Function::Create(
      funcType, llvm::Function::ExternalLinkage, "main", TheModule.get());
  llvm::BasicBlock::Create(TheContext, "entrypoint", mainFunc);
  ident_table.enterBlock();
  block(Program->getBlock(), mainFunc);
  TheBuilder.CreateRet(TheBuilder.getInt64(1));
}

void CodeGen::block(std::unique_ptr<BlockAST> block_ast, llvm::Function *func) {
  std::vector<std::string> vars;

  TheBuilder.SetInsertPoint(&func->getEntryBlock());
  constant(block_ast->getConstant());
  variable(block_ast->getVariable());
  auto functions = block_ast->getFunctions();
  for(size_t i = 0; i < functions.size(); i++) {
    function(std::move(functions[i]));
  }
  curFunc = func;
  TheBuilder.SetInsertPoint(&func->getEntryBlock());
  auto itr = func->arg_begin();
  for (size_t i = 0; i < func->arg_size(); i++) {
    auto *alloca =
        TheBuilder.CreateAlloca(TheBuilder.getInt64Ty(), 0, itr->getName());
    TheBuilder.CreateStore(itr, alloca);
    ident_table.appendParam(itr->getName(), alloca);
    itr++;
  }
  statement(block_ast->getStatement());
  ident_table.leaveBlock();
}

void CodeGen::constant(std::unique_ptr<ConstDeclAST> const_ast) {
  if (const_ast == nullptr) return;
  for (auto pair : const_ast->getNameTable())
    ident_table.appendConst(pair.first, TheBuilder.getInt64(pair.second));
}

void CodeGen::variable(std::unique_ptr<VarDeclAST> var_ast) {
  if (var_ast == nullptr) return;
  for (auto name : var_ast->getNameTable()) {
    auto *alloca = TheBuilder.CreateAlloca(TheBuilder.getInt64Ty(), 0, name);
    ident_table.appendVar(name, alloca);
  }
}

void CodeGen::function(std::unique_ptr<FuncDeclAST> func_ast) {
  if (func_ast == nullptr) return;
  auto func_name = func_ast->getName();
  auto params = func_ast->getParameters();
  std::vector<llvm::Type *> param_types(params.size(), TheBuilder.getInt64Ty());
  auto *funcType =
      llvm::FunctionType::get(TheBuilder.getInt64Ty(), param_types, false);
  auto *func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, func_name, TheModule.get());
  llvm::BasicBlock::Create(TheContext, "entry", func);
  ident_table.appendFunction(func_name, func);
  ident_table.enterBlock();
  auto itr = func->arg_begin();
  for (size_t i = 0; i < params.size(); i++) {
    itr->setName(params[i]);
    itr++;
  }

  block(func_ast->getBlock(), func);
}

void CodeGen::statement(std::unique_ptr<BaseStmtAST> stmt_ast) {
  if (stmt_ast == nullptr) return;
  if (llvm::isa<NullAST>(stmt_ast))
    return;
  else if (llvm::isa<AssignAST>(stmt_ast)) {
    statementAssign(llvm::cast<AssignAST>(std::move(stmt_ast)));
  } else if (llvm::isa<BeginEndAST>(stmt_ast)) {
    auto stmts = llvm::cast<BeginEndAST>(std::move(stmt_ast))->getStatements();
    for (size_t i=0; i < stmts.size(); i++)
      statement(std::move(stmts[i]));
  } else if (llvm::isa<IfThenAST>(std::move(stmt_ast))) {
    statementIf(llvm::cast<IfThenAST>(std::move(stmt_ast)));
  } else if (llvm::isa<WhileDoAST>(stmt_ast)) {
    statementWhile(llvm::cast<WhileDoAST>(std::move(stmt_ast)));
  } else if (llvm::isa<ReturnAST>(stmt_ast)) {
    auto exp_ast = llvm::cast<ReturnAST>(std::move(stmt_ast))->getExpression();
    TheBuilder.CreateRet(expression(std::move(exp_ast)));
    TheBuilder.SetInsertPoint(llvm::BasicBlock::Create(TheContext, "dummy"));
  } else if (llvm::isa<WriteAST>(stmt_ast)) {
    auto exp_ast = llvm::cast<WriteAST>(std::move(stmt_ast))->getExpression();
    TheBuilder.CreateCall(writeFunc, std::vector<llvm::Value *>(1, expression(std::move(exp_ast))));
  } else if (llvm::isa<WritelnAST>(stmt_ast)) {
    TheBuilder.CreateCall(writelnFunc);
  }
}

void CodeGen::statementAssign(std::unique_ptr<AssignAST> stmt_ast) {
  const auto &info = ident_table.find(stmt_ast->getName());
  llvm::Value *assignee = nullptr;
  if (info.type == IdType::VAR) {
    assignee = info.val;
  } else {
    Log::error("variable is expected but it is not variable");
    return;
  }
  TheBuilder.CreateStore(expression(stmt_ast->getRHS()), assignee);
}

void CodeGen::statementIf(std::unique_ptr<IfThenAST> stmt_ast) {
  auto cond_ast = stmt_ast->getCondition();
  auto *cond = condition(llvm::cast<CondExpAST>(std::move(cond_ast)));
  auto *then_block = llvm::BasicBlock::Create(TheContext, "if.then", curFunc);
  auto *merge_block = llvm::BasicBlock::Create(TheContext, "if.merge");
  TheBuilder.CreateCondBr(cond, then_block, merge_block);

  TheBuilder.SetInsertPoint(then_block);
  auto thenstmt = stmt_ast->getStatement();
  statement(std::move(thenstmt));
  TheBuilder.CreateBr(merge_block);
  then_block = TheBuilder.GetInsertBlock();
  curFunc->getBasicBlockList().push_back(merge_block);
  TheBuilder.SetInsertPoint(merge_block);
}

void CodeGen::statementWhile(std::unique_ptr<WhileDoAST> stmt_ast) {
  auto *cond_block = llvm::BasicBlock::Create(TheContext, "while.cond", curFunc);
  auto *body_block = llvm::BasicBlock::Create(TheContext, "while.body");
  auto *merge_block = llvm::BasicBlock::Create(TheContext, "while.merge");

  TheBuilder.CreateBr(cond_block);
  {
    TheBuilder.SetInsertPoint(cond_block);
    auto cond_ast = stmt_ast->getCondition();
    auto *cond = condition(llvm::cast<CondExpAST>(std::move(cond_ast)));
    TheBuilder.CreateCondBr(cond, body_block, merge_block);
  }
  {
    curFunc->getBasicBlockList().push_back(body_block);
    TheBuilder.SetInsertPoint(body_block);
    auto dostmt = stmt_ast->getStatement();
    statement(std::move(dostmt));
    TheBuilder.CreateBr(cond_block);
  }
  curFunc->getBasicBlockList().push_back(merge_block);
  TheBuilder.SetInsertPoint(merge_block);
}

llvm::CmpInst::Predicate CodeGen::token_to_inst(std::string op) {
  if (op == "=")
    return llvm::CmpInst::Predicate::ICMP_EQ;
  else if (op == "<>")
    return llvm::CmpInst::Predicate::ICMP_NE;
  else if (op == "<")
    return llvm::CmpInst::Predicate::ICMP_SLT;
  else if (op == "<=")
    return llvm::CmpInst::Predicate::ICMP_SLE;
  else if (op == ">")
    return llvm::CmpInst::Predicate::ICMP_SGT;
  else if (op == ">=")
    return llvm::CmpInst::Predicate::ICMP_SGE;
  else
    Log::error("not support at token to inst");
  return llvm::CmpInst::Predicate::FCMP_FALSE;
}

llvm::Value *CodeGen::condition(std::unique_ptr<CondExpAST> exp_ast) {
  auto op = exp_ast->getOp();
  if (op == "odd") {
    auto *rhs = TheBuilder.CreateSRem(expression(exp_ast->getRHS()), TheBuilder.getInt64(2));
    return TheBuilder.CreateICmpEQ(rhs, TheBuilder.getInt64(1));
  } else {
    auto *lhs = expression(exp_ast->getLHS());
    llvm::CmpInst::Predicate inst = token_to_inst(op);
    auto *rhs = expression(exp_ast->getRHS());
    return TheBuilder.CreateICmp(inst, lhs, rhs);
  }
}

llvm::Value *CodeGen::expression(std::unique_ptr<BaseExpAST> exp_ast) {
  if (llvm::isa<BinaryExprAST>(exp_ast))
    return binaryExp(llvm::cast<BinaryExprAST>(std::move(exp_ast)));
  else if (llvm::isa<CallExprAST>(exp_ast))
    return callExp(llvm::cast<CallExprAST>(std::move(exp_ast)));
  else if (llvm::isa<VariableAST>(exp_ast))
    return variableExp(llvm::cast<VariableAST>(std::move(exp_ast)));
  else if (llvm::isa<NumberAST>(exp_ast))
    return numberExp(llvm::cast<NumberAST>(std::move(exp_ast)));
  return nullptr;
}

llvm::Value *CodeGen::binaryExp(std::unique_ptr<BinaryExprAST> exp_ast) {
  auto op = exp_ast->getOp();
  llvm::Value *lhs = expression(exp_ast->getLHS());
  llvm::Value *rhs = expression(exp_ast->getRHS());
  if (exp_ast->getPrefix() == "-")
    lhs = TheBuilder.CreateNeg(lhs);
  if (op == "+")
    lhs = TheBuilder.CreateAdd(lhs, rhs);
  else if (op == "-")
    lhs = TheBuilder.CreateSub(lhs, rhs);
  else if (op == "*")
    lhs = TheBuilder.CreateMul(lhs, rhs);
  else if (op == "/")
    lhs = TheBuilder.CreateSDiv(lhs, rhs);

  return lhs;
}

llvm::Value *CodeGen::callExp(std::unique_ptr<CallExprAST> exp_ast) {
  auto &val = ident_table.find(exp_ast->getCallee());
  std::vector<llvm::Value *> args;
  for (size_t i = 0; i < exp_ast->getArgSize(); i++) {
    args.push_back(expression(exp_ast->getArgs(i)));
  }
  if (args.size() != val.func->arg_size()) {
    Log::error("argument number is wrong");
    return nullptr;
  }
  return TheBuilder.CreateCall(val.func, args);

}

llvm::Value *CodeGen::variableExp(std::unique_ptr<VariableAST> exp_ast) {
  auto &val = ident_table.find(exp_ast->getName());
  switch (val.type) {
  case IdType::CONST:
    return val.val;
  case IdType::VAR:
    return TheBuilder.CreateLoad(val.val);
  case IdType::PARAM:
    return TheBuilder.CreateLoad(val.val);
  default:
    ; // for not warning
  }
  return nullptr;
}

llvm::Value *CodeGen::numberExp(std::unique_ptr<NumberAST> exp_ast) {
  return TheBuilder.getInt64(exp_ast->getNumberValue());
}

void CodeGen::setLibraries() {
    // declare int printf(*char, ...)
  std::vector<llvm::Type *> Int8s(1, TheBuilder.getInt8PtrTy());
  auto *printfFT =
    llvm::FunctionType::get(TheBuilder.getInt32Ty(), Int8s, true);
  auto *printfFunc = llvm::Function::Create(
        printfFT, llvm::Function::ExternalLinkage, "printf", TheModule.get());

  // define void write(int i)
  std::vector<llvm::Type *> Int32s(1, TheBuilder.getInt64Ty());
  auto *writeFT =
    llvm::FunctionType::get(TheBuilder.getVoidTy(), Int32s, false);
  writeFunc = llvm::Function::Create(
        writeFT, llvm::Function::ExternalLinkage, "write", TheModule.get());

  auto Arg = writeFunc->arg_begin();
  Arg->setName("i");

  auto *writeBB = llvm::BasicBlock::Create(TheContext, "entry", writeFunc);
  TheBuilder.SetInsertPoint(writeBB);

  auto *writeStr = TheBuilder.CreateGlobalString("%d\n", ".str");

  auto *alloca = TheBuilder.CreateAlloca(TheBuilder.getInt64Ty(), 0, "i.addr");
  TheBuilder.CreateStore(Arg, alloca);
  auto *param = TheBuilder.CreateLoad(alloca, "param");
  auto* zero = TheBuilder.getInt32(0);
  llvm::Value* args[] = {zero, zero};
  auto *gep = TheBuilder.CreateInBoundsGEP(writeStr, args, "gep");
  llvm::Value* write_args[] = { gep, param };
  TheBuilder.CreateCall(printfFunc, write_args, "call_write");
  TheBuilder.CreateRetVoid();

  // define void writeln()
  auto *writelnFT =
    llvm::FunctionType::get(TheBuilder.getVoidTy(), false);
  writelnFunc = llvm::Function::Create(
        writelnFT, llvm::Function::ExternalLinkage, "writeln", TheModule.get());

  auto *writelnBB = llvm::BasicBlock::Create(TheContext, "entry", writelnFunc);
  TheBuilder.SetInsertPoint(writelnBB);

  auto *writelnStr = TheBuilder.CreateGlobalString("\n", ".str");

  gep = TheBuilder.CreateInBoundsGEP(writelnStr, args, "gep");
  llvm::Value *writeln_args[] = { gep };
  TheBuilder.CreateCall(printfFunc, writeln_args, "call_writeln");
  TheBuilder.CreateRetVoid();
}
