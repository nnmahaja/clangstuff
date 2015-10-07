#ifndef CLANG_UTILS_H
#define CLANG_UTILS_H

#include "clang/AST/AST.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

namespace kanor {
  namespace clang_util {
    std::string unparse(const clang::Stmt* stmt) {
      std::string temp;
      llvm::raw_string_ostream str_o(temp);

      stmt->printPretty(str_o, 0, clang::PrintingPolicy(clang::LangOptions()));

      return str_o.str();
    }

    std::string unparse(const clang::Decl* decl) {
      std::string temp;
      llvm::raw_string_ostream str_o(temp);

      decl->print(str_o);

      return str_o.str();
    }
    
    std::string get_var_name(const clang::Decl* decl) {
      if (const clang::VarDecl* vdecl = llvm::dyn_cast<clang::VarDecl>(decl)) {
	return vdecl->getNameAsString();
      }
      return std::string("");
    }
    
    inline
    std::string get_fun_name(const clang::CallExpr* call) {
      return call->getDirectCallee()->getNameAsString();
    }
    
    // TODO write a nodequery method similar to ROSE that returns a vector of
    // specified node type
  }
}

#endif