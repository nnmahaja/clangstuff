#include "Analysis/cfg_dump.h"

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

bool FuncVisitor::VisitFunctionDecl(clang::FunctionDecl* fdecl) {
    
  if (!fdecl->hasBody()) return true;
  if (!_rewriter.getSourceMgr().isInMainFile(fdecl->getLocStart())) return true;
  
//   std::unique_ptr<clang::CFG> fn_cfg = clang::CFG::buildCFG(fdecl, fdecl->getBody(), ast_ctx, clang::CFG::BuildOptions());
//   fn_cfg->dump(LangOptions(), true);
  
  fdecl->getBody()->dump();
  
  return true;
}

// int main(int argc, const char **argv) {
//   llvm::cl::OptionCategory h_inf_category("Hint Inference");
//   CommonOptionsParser op(argc, argv, h_inf_category);
//   ClangTool Tool(op.getCompilations(), op.getSourcePathList());
// 
//   // ClangTool::run accepts a FrontendActionFactory, which is then used to
//   // create new objects implementing the FrontendAction interface. Here we use
//   // the helper newFrontendActionFactory to create a default factory that will
//   // return a new MyFrontendAction object every time.
//   // To further customize this, we could create our own factory class.
//   return Tool.run(newFrontendActionFactory<KFrontendAction>().get());
// }
