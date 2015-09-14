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

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

class DeclOrStmtPtr {
private:
  clang::Decl* decl;
  clang::Stmt* stmt;
public:
  typedef enum {DECL, STMT} value_type;
  value_type type;
  DeclOrStmtPtr(clang::Decl* v) : decl(v), stmt(nullptr), type(DECL) {}
  DeclOrStmtPtr(clang::Stmt* v) : decl(nullptr), stmt(v), type(STMT) {}
  clang::Decl* getAsDecl() {
    return decl;
  }
  clang::Stmt* getAsStmt() {
    return stmt;
  }
};
using defs_set = llvm::DenseMap< const clang::Stmt*, const clang::Decl* >;
defs_set CollectAllDefs(const std::unique_ptr<clang::CFG>& fn_cfg) {
  
  defs_set defs;
  auto call_back = [&](const Stmt* stmt) {
    if (const clang::DeclStmt* dstmt = 
	llvm::dyn_cast<clang::DeclStmt>(stmt)) {
      if (dstmt->isSingleDecl()) {
	llvm::outs() << "adding decl\n";
	defs[dstmt] = dstmt->getSingleDecl();
      }
    }
    
    const clang::BinaryOperator* bop = llvm::dyn_cast<clang::BinaryOperator>(stmt);
    if (bop && (bop->getOpcode() == BO_Assign)) {
      
      if (auto dre = llvm::dyn_cast<clang::DeclRefExpr>(bop->getLHS())) {
	llvm::outs() << "adding assignment\n";
	defs[bop] = dre->getDecl();
      }
      
    }
    
  };
  fn_cfg->VisitBlockStmts(call_back);
  
  return defs;
}

class FuncVisitor : public RecursiveASTVisitor<FuncVisitor> {
public:
  
  FuncVisitor(Rewriter &R, ASTContext* actx) : TheRewriter(R), ast_ctx(actx) {}
  
  bool VisitFunctionDecl(clang::FunctionDecl* fdecl) {
    
    if (!fdecl->hasBody()) return true;
    if (!TheRewriter.getSourceMgr().isInMainFile(fdecl->getLocStart())) return true;
    
    std::unique_ptr<clang::CFG> fn_cfg = clang::CFG::buildCFG(fdecl, fdecl->getBody(), ast_ctx, clang::CFG::BuildOptions());
    
    
    fn_cfg->dump(LangOptions(), true);
    
//     defs_set defs = CollectAllDefs(fn_cfg);
    
    return true;
  }
  
private:
  Rewriter &TheRewriter;
  ASTContext* ast_ctx;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer(Rewriter &R, ASTContext* actx) : visitor(R, actx) {}

  // Override the method that gets called for each parsed top-level
  // declaration.
  bool HandleTopLevelDecl(DeclGroupRef DR) override {
    for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
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
class KFrontendAction : public ASTFrontendAction {
public:
  KFrontendAction() {}
    
  void EndSourceFileAction() override {
    // Now emit the rewritten buffer.
//     TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID()).write(llvm::outs());
  }

  std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return llvm::make_unique<MyASTConsumer>(TheRewriter, &(CI.getASTContext()));
  }

private:
  Rewriter TheRewriter;
};

int main(int argc, const char **argv) {
  llvm::cl::OptionCategory h_inf_category("Hint Inference");
  CommonOptionsParser op(argc, argv, h_inf_category);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  // ClangTool::run accepts a FrontendActionFactory, which is then used to
  // create new objects implementing the FrontendAction interface. Here we use
  // the helper newFrontendActionFactory to create a default factory that will
  // return a new MyFrontendAction object every time.
  // To further customize this, we could create our own factory class.
  return Tool.run(newFrontendActionFactory<KFrontendAction>().get());
}
