#include <sstream>
#include <string>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/Analyses/Dominators.h"

#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

class FuncVisitor : public clang::RecursiveASTVisitor<FuncVisitor> {
public:
  
  FuncVisitor(clang::Rewriter &R, clang::ASTContext* actx) : TheRewriter(R), ast_ctx(actx) {}
  bool VisitFunctionDecl(clang::FunctionDecl* fdecl);
  
private:
  clang::Rewriter &_rewriter;
  clang::ASTContext* _ast_ctx;
};

class MyASTConsumer : public clang::ASTConsumer {
public:
  MyASTConsumer(clang::Rewriter &R, clang::ASTContext* actx) : visitor(R, actx) {}

  // Override the method that gets called for each parsed top-level
  // declaration.
  bool HandleTopLevelDecl(clang::DeclGroupRef DR) override {
    for (clang::DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
      // Traverse the declaration using our AST visitor.
      visitor.TraverseDecl(*b);
    }
    return true;
  }
  
  // This is called at the end of parsed file
  // Does not contain definitions from headers e.g. the overloaded | op
  /*
  virtual void HandleTranslationUnit(clang::ASTContext &Context) {
    // Traversing the translation unit decl via a RecursiveASTVisitor
    // will visit all nodes in the AST.
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }*/

private:
  FuncVisitor visitor;
};

// For each source file provided to the tool, a new FrontendAction is created.
class KFrontendAction : public clang::ASTFrontendAction {
public:
  KFrontendAction() {}
    
  void EndSourceFileAction() override {
    // Now emit the rewritten buffer.
//     TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID()).write(llvm::outs());
  }

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, StringRef file) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return llvm::make_unique<MyASTConsumer>(TheRewriter, &(CI.getASTContext()));
  }

private:
  clang::Rewriter _rewriter;
};