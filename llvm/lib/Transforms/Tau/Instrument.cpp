//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h" // For errs(), I think
#include "llvm/IR/IRBuilder.h" // IRBuilder
#include "llvm/IR/Constants.h" // To create string literals in the AST

// Need these to use Sampson's registration technique
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/LegacyPassManager.h"

#ifdef TAU_PROF_CXX
#include <cxxabi.h>
#endif

// Other passes do this, so I assume the macro is useful somewhere
#define DEBUG_TYPE "tau-profile"



using namespace llvm;

namespace {

// technique borrowed/modified from
// https://github.com/eklitzke/demangle/blob/master/src/demangle.cc
static StringRef normalize_name(StringRef mangled_name) {
#ifdef TAU_PROF_CXX
  int status = 0;

  const char *str = abi::__cxa_demangle(mangled_name.begin(), 0, 0, &status);
  StringRef realname{str};

  switch (status) {
  case 0:
    break;
  case -1:
    errs() << "FAIL: failed to allocate memory while demangling "
           << mangled_name << '\n';
    break;
  case -2:
    errs() << "FAIL: " << mangled_name
           << " is not a valid name under the C++ ABI mangling rules\n";
    break;
  default:
    errs() << "FAIL: couldn't demangle " << mangled_name
           << " for some unknown reason: " << status << '\n';
    break;
  }

  return realname;
#else
  return mangled_name;
#endif
}

  /*!
   *  Find/create a function taking a single `i8*` argument with a void return
   *  type suitable for making a call to in IR.
   *
   * \param funcname The name of the function
   * \param ctx The LLVMContext
   * \param mdl The Module in which the function will be used
   */
  static Constant *getVoidFunc(StringRef funcname, LLVMContext &context, Module *module) {

    // Void return type
    Type *retTy = Type::getVoidTy(context);

    // single i8* argument type (char *)
    Type *argTy = Type::getInt8PtrTy(context);
    std::vector<Type *> paramTys{argTy};

    // Third param to `get` is `isVarArg`.  It's not documented, but might have
    // to do with variadic functions?
    FunctionType *funcTy = FunctionType::get(retTy, paramTys, false);
    return module->getOrInsertFunction(funcname, funcTy);
  }


  /*!
   * The instrumentation pass.
   */
  struct Instrument : public FunctionPass {

    static char ID; // Pass identification, replacement for typeid

    Instrument() : FunctionPass(ID) {}

    bool runOnFunction(Function &func) override {

      bool mutated = false;

      // Declare and get handles to the runtime profiling functions
      auto &context = func.getContext();
      auto *module = func.getParent();
      Constant
        *onCallFunc = getVoidFunc("tau_prof_func_call", context, module),
        *onRetFunc = getVoidFunc("tau_prof_func_ret", context, module);

      // vector to save references to the calls we want to instrument
      // TODO: Follow the suggestions of the LLVM programmer guide re: containers
      std::vector<CallInst*> calls;

      for (auto &block : func) {
        for (auto &inst : block) {

          if(auto *op = dyn_cast<CallInst>(&inst)) {
            // Inserting new function calls here will modify the block and
            // potentially give us a never-ending list of calls to instrument
            // (without a white/blacklist in place).  It's simpler to just
            // gather them up and mess with them afterwards.
            calls.push_back(op);
          }
        }
      }

      for(auto *op : calls) {
        errs() << "Found a CallInst with type " << *op->getFunctionType() << '\n';
        auto *callee = op->getCalledFunction();

        if(!callee) {
          errs() << "CallInst is INDIRECT.  I can't do anything about that.\n";
          continue;
        }

        StringRef calleeName = normalize_name(callee->getName());

        if(calleeName.empty()) {
          // Good for a call whose name is not mangled but we still want to instrument;
          calleeName = callee->getName();
        }

        IRBuilder<> builder(op);

        // This is the recommended way of creating a string constant (to be used
        // as an argument to runtime functions)
        Value *strArg = builder.CreateGlobalStringPtr(calleeName);

        errs() << "Function name is " << calleeName << '\n';

        if(!shouldInstrument(calleeName)) {
          errs() << "Should *not* instrument\n";
          continue;
        }

        errs() << "*Should* instrument\n";

        std::vector<Value *> args{strArg};

        // Before the CallInst
        builder.CreateCall(onCallFunc, args);

        // Set insert point to just after the CallInst
        builder.SetInsertPoint(op->getParent(), ++builder.GetInsertPoint());
        builder.CreateCall(onRetFunc, args);

        mutated = true;
        }

      return mutated;
    }

    bool shouldInstrument(StringRef) { return true; }
  };


}

char Instrument::ID = 0;

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerInstrumentPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  PM.add(new Instrument());
}
static RegisterStandardPasses
RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible, registerInstrumentPass);