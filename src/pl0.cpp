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
#include "ast.hpp"
#include "parser.hpp"
#include "codegen.hpp"
#include "error.hpp"

llvm::cl::opt<bool> debug("d", llvm::cl::desc("Enable debug"), llvm::cl::value_desc("filename"));
llvm::cl::opt<std::string> InputFileName(llvm::cl::Positional, llvm::cl::desc("<input file>"), llvm::cl::Required);

/**
 * main関数
 */
int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

/*
auto Tokens = LexicalAnalysis(InputFileName);

if (Tokens)
  Tokens->printTokens();
*/

  auto TheParser = llvm::make_unique<Parser>(InputFileName, debug);
  if (!TheParser->doParse()) {
    fprintf(stderr, "err at parser or lexer\n");
    exit(1);
  }

  //get AST
  auto TheProgramAST = TheParser->getAST();
  if (!TheProgramAST) {
    fprintf(stderr,"Program is empty");
    exit(1);
  }

  auto TheCodegen = llvm::make_unique<CodeGen>(InputFileName);

  TheCodegen->generate(std::move(TheProgramAST));

  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  auto TheModule = TheCodegen->getModule();

  auto triple = llvm::sys::getDefaultTargetTriple();
  TheModule->setTargetTriple(triple);

  std::string err;
  auto target = llvm::TargetRegistry::lookupTarget(triple, err);
  if (!target)
    error(err.c_str());

  auto cpu = "generic";
  auto features = "";
  llvm::TargetOptions option;
  auto rm = llvm::Optional<llvm::Reloc::Model>();
  auto machine = target->createTargetMachine(
    triple, cpu, features, option, rm);

  TheModule->setDataLayout(machine->createDataLayout());

  int ext = InputFileName.find_last_of(".");
  auto prog_name = (InputFileName.substr(0, ext) + ".o").c_str();
  std::error_code err_code;
  llvm::raw_fd_ostream dest(prog_name, err_code, llvm::sys::fs::F_None);
  if (err_code) {
    error(("Could not open output file: " + err_code.message()).c_str());
  }

  auto ThePM = llvm::legacy::PassManager();
  ThePM.add(llvm::createPromoteMemoryToRegisterPass());
  ThePM.add(llvm::createInstructionCombiningPass());
  ThePM.add(llvm::createReassociatePass());
  ThePM.add(llvm::createGVNPass());
  ThePM.add(llvm::createUnifyFunctionExitNodesPass());
  ThePM.add(llvm::createCFGSimplificationPass());

  auto file_type = llvm::TargetMachine::CGFT_ObjectFile;
  if (machine->addPassesToEmitFile(ThePM, dest, nullptr, file_type))
    error("TheTargetMachine can't emit a file of this type");
  ThePM.run(*TheModule);
  dest.flush();

  return 0;
}
