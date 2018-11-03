#include "llvm/Support/CommandLine.h"
#include "llvm/ADT/Optional.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "lexer.hpp"
#include <iostream>
/*
#include "AST.hpp"
#include "parser.hpp"
#include "codegen.hpp"
#include "pl0.hpp"


extern std::unique_ptr<llvm::Module> TheModule;
std::unique_ptr<Parser> TheParser;
std::unique_ptr<ProgramAST> TheProgramAST;
std::unique_ptr<CodeGen> TheCodegen;
*/
llvm::cl::opt<std::string> OutputFilename("o", llvm::cl::desc("Specify output filename"), llvm::cl::value_desc("filename"));
llvm::cl::opt<std::string> InputFilename(llvm::cl::Positional, llvm::cl::desc("<input file>"), llvm::cl::Required);

/**
 * main関数
 */
int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

auto Tokens = LexicalAnalysis(InputFilename);

if (Tokens)
  Tokens->printTokens();

/*
  TheParser = llvm::make_unique<Parser>(InputFilename);
  if (!TheParser->doParse()) {
    fprintf(stderr, "err at parser or lexer\n");
    exit(1);
  }

  //get AST
  TheProgramAST = TheParser->getAST();
  if(!TheProgramAST){
    fprintf(stderr,"Program is empty");
    exit(1);
  }

  TheCodegen = llvm::make_unique<CodeGen>();
  if (!TheCodegen->doCodeGen(TheProgramAST, InputFileName)) {
    fprintf(stderr, "err at codegen\n");
    exit(1);
  }

  llvm::legacy::PassManager ThePM = llvm::legacy::PassManager();
  ThePM.add(llvm::createInstructionCombiningPass());
  // 式の再結合
  ThePM.add(llvm::createReassociatePass());
  // 共通部分式の消去
  ThePM.add(llvm::createGVNPass());
  // 制御フロー図の簡約化 (到達不能ブロックの削除など).
  ThePM.add(llvm::createCFGSimplificationPass());
  ThePM.run(*TheModule);

  TheModule->dump();
*/
  return 0;
}
