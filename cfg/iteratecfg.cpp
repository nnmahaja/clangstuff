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

using vardeclset_type = llvm::SmallPtrSet< clang::VarDecl*, 30 >;

/*
 * This class is used to collect free vars in a communication statement.
 * The free variables do not include variables bound in generators (_for_each).
 */
class CommStmtWalker : public RecursiveASTVisitor<CommStmtWalker> {
  
private:
  vardeclset_type gen_bound_vars;
  vardeclset_type stmt_free_vars;
  
public:
  
  // collects gen_bound_vars
  bool VisitCallExpr(clang::CallExpr* call) {
    
    if(call->getDirectCallee()->getNameAsString().find("_for_each") != std::string::npos) {
      auto first_arg = call->getArg(0);
      if (clang::DeclRefExpr* use = clang::dyn_cast< clang::DeclRefExpr >(first_arg)) {
	if(clang::VarDecl* vdecl = clang::dyn_cast< clang::VarDecl >(use->getDecl())) {
	  gen_bound_vars.insert(vdecl);
	}
      }
    }
    return true;
  }
  
  // collects stmt_free_vars
  bool VisitDeclRefExpr(clang::DeclRefExpr* use) {
    
    if(clang::VarDecl* vdecl = clang::dyn_cast<clang::VarDecl>(use->getDecl())) {
      std::string str_type = vdecl->getType().getAsString();
      // FIXME: Need to identify other topologies
      if ((str_type.find("kanor::MPI_OneToOne") == std::string::npos) &&
	  (str_type.find("kanor::internal::hint_type") == std::string::npos)) {
	stmt_free_vars.insert(vdecl);
      }
    }
    return true;
  }
  
  // stmt_free_vars - gen_bound_vars
  vardeclset_type get_free_vars() const {
    vardeclset_type fv;
    for (auto vd : stmt_free_vars) {
      if (std::find(gen_bound_vars.begin(), gen_bound_vars.end(), vd) == gen_bound_vars.end()) {
	fv.insert(vd);
      }
    }
    return fv;
  }
};

// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.
class FVCollector : public RecursiveASTVisitor<FVCollector> {
public:
  
  FVCollector(Rewriter &R, ASTContext* actx) : TheRewriter(R), ast_ctx(actx) {}

  std::map< clang::CXXOperatorCallExpr*, vardeclset_type > stmt_fv_map;
  
  
  bool VisitCXXOperatorCallExpr(clang::CXXOperatorCallExpr* op_call) {
    
//     if (!TheRewriter.getSourceMgr().isInMainFile(op_call->getLocStart())) return true;
    
    if (clang::OO_Pipe == op_call->getOperator()) {
      FunctionDecl* callee = op_call->getDirectCallee();
      if (callee) {
	StringRef fname_ref = TheRewriter.getSourceMgr().getFilename(callee->getLocStart());
	if(fname_ref.find("op_impl.h") != clang::StringRef::npos) {
	  // this is the overloaded | operator
	  // whose parent is ExprWithCleanups
	  CommStmtWalker stmt_walker;
	  stmt_walker.TraverseStmt(op_call);
	  auto free_vars = stmt_walker.get_free_vars();
	  stmt_fv_map[op_call] = free_vars;
	}
      }
    }
    
    return true;
  }
  
private:
  Rewriter &TheRewriter;
  ASTContext* ast_ctx;
};

using stmt_varref_map = llvm::DenseMap< const Stmt*, llvm::SmallPtrSet< clang::DeclRefExpr*, 30 > >;

struct DeclRefVisitor : public RecursiveASTVisitor<DeclRefVisitor> {
  llvm::SmallPtrSet< clang::DeclRefExpr*, 30 > uses;
  bool VisitDeclRefExpr(clang::DeclRefExpr* dref) {
    uses.insert(dref);
    return true;
  }
};

class FuncVisitor : public RecursiveASTVisitor<FuncVisitor> {
public:
  using dtnb_type = llvm::DomTreeNodeBase<clang::CFGBlock>;
  FuncVisitor(Rewriter &R, ASTContext* actx) : TheRewriter(R), ast_ctx(actx) {}
  
  // traverse dtree in preorder and put the results in visited
  void pre(dtnb_type* curr, 
	   std::deque< dtnb_type* >& visited) const {
    visited.push_back(curr);
    for (dtnb_type* b : *(curr)) {
      pre(b, visited);
    }
  }
  
  void CalculateControlDependence(const std::unique_ptr<clang::CFG>& fn_cfg) const {
    std::deque<CFGBlock*> wlist;
    std::copy(fn_cfg->begin(), fn_cfg->end(), std::back_inserter(wlist));
    /*
    const CFGBlock entry = fn_cfg->getEntry();
    const CFGBlock exit = fn_cfg->getExit();
    for (CFG::const_iterator itr = fn_cfg->begin(); itr != fn_cfg->end(); ++itr) {
      const CFGBlock blk = *itr;
    }
    
    while(!wlist.empty()) {
      CFGBlock* blk = wlist.front();
      wlist.pop_front();
      
      
      // if dataflow state changes, add successors
      for (const CFGElement e : *blk) {
	switch(e.getKind()) {
	  case CFGElement::Statement: {
	    break;
	  }
	  default: {
	    llvm_unreachable("CFGElement");
	  }
	}
      }
    }
    */
    
//     DominatorTree dt;
//     AnalysisDeclContextManager adcm;
//     AnalysisDeclContext adc(&adcm, fdecl);
//     dt.buildDominatorTree(adc);
//     fn_cfg.get()->print(llvm::outs(), LangOptions(), true);
//     llvm::outs() << "\n";
    llvm::DominatorTreeBase< clang::CFGBlock > pdomtree(true);
    pdomtree.recalculate(*(fn_cfg.get()));
//     pdomtree.print(llvm::outs());
    /*
    CFGBlock* root = pdomtree.getRoot();
    llvm::outs() << "root:\n";
    root->dump();
    llvm::outs() << "\n";
    llvm::SmallVector< CFGBlock*, 10 > desc;
    pdomtree.getDescendants(root, desc);
    llvm::outs() << "desc:\n";
    for (auto p : desc) {
      p->dump();
    }
    llvm::outs() << "----------------------\n\n";
    */
    
    std::deque< dtnb_type* > pre_visit;
    // topologically sort postdom tree, which is equivalent to preorder traversal
    pre(pdomtree.getRootNode(), pre_visit);
    
    std::reverse(pre_visit.begin(), pre_visit.end());
    llvm::outs() << "preorder: ";
    for (auto bid : pre_visit) {
      llvm::outs() << bid->getBlock()->getBlockID() << " ";
    }
    llvm::outs() << "\n";
    
    // Allen Kennedy algo
    llvm::DenseMap< dtnb_type*, llvm::SmallPtrSet< dtnb_type*, 20 > > CD;
    while (!pre_visit.empty()) {
      dtnb_type* x = pre_visit.front();
      pre_visit.pop_front();
      
      for (CFGBlock::const_pred_iterator p_itr = x->getBlock()->pred_begin(); p_itr != x->getBlock()->pred_end(); ++p_itr) {
	CFGBlock* p = *p_itr;
	dtnb_type* y = pdomtree.getNode(p);
	if (y->getIDom() != x) CD[x].insert(y);
      }
      
      for (dtnb_type* z : *x) {
	for (dtnb_type* y : CD[z]) {
	  if (y->getIDom() != x) CD[x].insert(y);
	}
      }
    }
    
    for (auto e : CD) {
      llvm::outs() << e.first->getBlock()->getBlockID() << ": ";
      for (auto cd : e.second) {
	llvm::outs() << cd->getBlock()->getBlockID() << " ";
      }
      llvm::outs() << "\n";
    }
  }
  
//   using const_stmt_set = llvm::SmallPtrSet< const clang::Stmt*, 20 >;
  using const_stmt_set = std::set< const clang::Stmt* >;
  using defs_map = llvm::DenseMap< const clang::Decl*, const_stmt_set >;
  
  defs_map CalculateGen(const clang::CFGBlock* bb) {
    
    defs_map gens;
    
    for (const clang::CFGElement e : *bb) {
      switch(e.getKind()) {
	case clang::CFGElement::Statement: {
	  
	  const Stmt *stmt = e.castAs<CFGStmt>().getStmt();
	  if (const clang::DeclStmt* dstmt = llvm::dyn_cast<clang::DeclStmt>(stmt)) {
	    if (dstmt->isSingleDecl()) {
	      const_stmt_set s; s.insert(dstmt);
	      gens[dstmt->getSingleDecl()] = s;
	    }
	  }
	  
	  if (const clang::BinaryOperator* bop = llvm::dyn_cast<clang::BinaryOperator>(stmt)) {
	    switch(bop->getOpcode()) {
	      case BO_Assign:
	      case BO_MulAssign: 
	      case BO_DivAssign:
	      case BO_RemAssign:
	      case BO_AddAssign:
	      case BO_SubAssign:
	      case BO_ShlAssign:
	      case BO_ShrAssign:
	      case BO_AndAssign:
	      case BO_XorAssign:
	      case BO_OrAssign: {
		if (auto dref = llvm::dyn_cast<clang::DeclRefExpr>(bop->getLHS())) {
		  const_stmt_set s; s.insert(bop);
		  gens[dref->getDecl()] = s;
		}
	      }
	      default: {} // do nothing
	    }
	  }
	  
	  if (const clang::UnaryOperator* uop = llvm::dyn_cast<clang::UnaryOperator>(stmt)) {
	    switch(uop->getOpcode()) {
	      case UO_PostInc:
	      case UO_PostDec:
	      case UO_PreInc:
	      case UO_PreDec: {
		if (clang::DeclRefExpr* var = llvm::dyn_cast<clang::DeclRefExpr>(uop->getSubExpr())) {
		  const_stmt_set s; s.insert(uop);
		  gens[var->getDecl()] = s;
		}
	      }
	      default: {}// do nothing
	    }
	  }
	  
	  break;
	}
	default: llvm_unreachable("cfg element not handled...");
      }
    }
    
    return gens;
  }
  
  defs_map CalculateKill(const clang::CFGBlock* bb, defs_map& all_defs) {
    defs_map kills;
    
    for (const clang::CFGElement e : *bb) {
      switch(e.getKind()) {
	case clang::CFGElement::Statement: {
	  
	  const Stmt *stmt = e.castAs<CFGStmt>().getStmt();
	  
	  if (const clang::DeclStmt* dstmt = llvm::dyn_cast<clang::DeclStmt>(stmt)) {
	    if (dstmt->isSingleDecl()) {
	      for (const clang::Stmt* d: all_defs[dstmt->getSingleDecl()]) {
		if (d != dstmt) kills[dstmt->getSingleDecl()].insert(d);
	      }
	    }
	  }
	  
	  if (const clang::BinaryOperator* bop = llvm::dyn_cast<clang::BinaryOperator>(stmt)) {
	    switch(bop->getOpcode()) {
	      case BO_Assign:
	      case BO_MulAssign: 
	      case BO_DivAssign:
	      case BO_RemAssign:
	      case BO_AddAssign:
	      case BO_SubAssign:
	      case BO_ShlAssign:
	      case BO_ShrAssign:
	      case BO_AndAssign:
	      case BO_XorAssign:
	      case BO_OrAssign: {
		if (auto dref = llvm::dyn_cast<clang::DeclRefExpr>(bop->getLHS())) {
		  for (const clang::Stmt* d: all_defs[dref->getDecl()]) {
		    if (d != bop) kills[dref->getDecl()].insert(d);
		  }
		}
	      }
	      default: {} // do nothing
	    }
	  }
	  
	  if (const clang::UnaryOperator* uop = llvm::dyn_cast<clang::UnaryOperator>(stmt)) {
	    switch(uop->getOpcode()) {
	      case UO_PostInc:
	      case UO_PostDec:
	      case UO_PreInc:
	      case UO_PreDec: {
		if (clang::DeclRefExpr* var = llvm::dyn_cast<clang::DeclRefExpr>(uop->getSubExpr())) {
		  for (const clang::Stmt* d: all_defs[var->getDecl()]) {
		    if (d != uop) kills[var->getDecl()].insert(d);
		  }
		}
	      }
	      default: {}// do nothing
	    }
	  }
	  
	  break;
	}
	default: llvm_unreachable("cfg element not handled...");
      }
    }
    
    return kills;
  }
  
  defs_map CollectAllDefs(const std::unique_ptr<clang::CFG>& fn_cfg) {
    
    defs_map defs;
    
    auto def_coll = [&](const Stmt* stmt) {
      if (const clang::DeclStmt* dstmt = llvm::dyn_cast<clang::DeclStmt>(stmt)) {
	if (dstmt->isSingleDecl()) {
	  defs[dstmt->getSingleDecl()].insert(dstmt);
	}
      }
      
      if (const clang::BinaryOperator* bop = llvm::dyn_cast<clang::BinaryOperator>(stmt)) {
	switch(bop->getOpcode()) {
	  case BO_Assign:
	  case BO_MulAssign: 
	  case BO_DivAssign:
	  case BO_RemAssign:
	  case BO_AddAssign:
	  case BO_SubAssign:
	  case BO_ShlAssign:
	  case BO_ShrAssign:
	  case BO_AndAssign:
	  case BO_XorAssign:
	  case BO_OrAssign: {
	    if (auto dre = llvm::dyn_cast<clang::DeclRefExpr>(bop->getLHS())) {
	      defs[dre->getDecl()].insert(bop);
	    }
	  }
	  default: {} // do nothing
	}
      }
      
      if (const clang::UnaryOperator* uop = llvm::dyn_cast<clang::UnaryOperator>(stmt)) {
	switch(uop->getOpcode()) {
	  case UO_PostInc:
	  case UO_PostDec:
	  case UO_PreInc:
	  case UO_PreDec: {
	    if (clang::DeclRefExpr* var = llvm::dyn_cast<clang::DeclRefExpr>(uop->getSubExpr())) {
	      defs[var->getDecl()].insert(uop);
	    }
	  }
	  default: {}// do nothing
	}
      }
      
    };
    fn_cfg->VisitBlockStmts(def_coll);
    
    return defs;
  }
  
  stmt_varref_map CollectUses(const std::unique_ptr<clang::CFG>& fn_cfg) {
    stmt_varref_map uses;
    
    auto use_coll = [&](Stmt* stmt) {
      
      if (const clang::BinaryOperator* bop = llvm::dyn_cast<clang::BinaryOperator>(stmt)) {
	switch(bop->getOpcode()) {
	  case BO_Assign:
	  case BO_MulAssign: 
	  case BO_DivAssign:
	  case BO_RemAssign:
	  case BO_AddAssign:
	  case BO_SubAssign:
	  case BO_ShlAssign:
	  case BO_ShrAssign:
	  case BO_AndAssign:
	  case BO_XorAssign:
	  case BO_OrAssign: {
	    DeclRefVisitor visitor;
	    visitor.TraverseStmt(bop->getRHS());
	    uses[stmt] = visitor.uses;
	  }
	  default: {} // do nothing
	}
	return;
      }
      
      if (const clang::UnaryOperator* uop = llvm::dyn_cast<clang::UnaryOperator>(stmt)) {
	switch(uop->getOpcode()) {
	  case UO_PostInc:
	  case UO_PostDec:
	  case UO_PreInc:
	  case UO_PreDec: {
	    if (clang::DeclRefExpr* var = llvm::dyn_cast<clang::DeclRefExpr>(uop->getSubExpr())) {
	      llvm::SmallPtrSet< clang::DeclRefExpr*, 30 > u;
	      u.insert(var);
	      uses[stmt] = u;
	    }
	  }
	  default: {}// do nothing
	}
	return;
      }
      
      DeclRefVisitor visitor;
      visitor.TraverseStmt(stmt);
    };
    fn_cfg->VisitBlockStmts(use_coll);
    
    return uses;
  }
  
  bool equals(defs_map& df1, defs_map& df2) {
    if (df1.size() != df2.size()) {
      return false;
    }
    // remember sets are sorted by guarantee
    for (auto d : df1) {
      const_stmt_set s1 = d.second;
      const_stmt_set s2 = df2[d.first];
      if (s1.size() != s2.size()) return false;
      auto itr1 = s1.begin(); auto itr2 = s2.begin();
      for (; itr1 != s1.end(); ++itr1, ++itr2) {
	const Stmt* stmt1 = *itr1; const Stmt* stmt2 = *itr2;
	if (stmt1 != stmt2) return false;
      }
    }
    return true;
  }
  
  // s1 - s2
  const_stmt_set set_diff(const const_stmt_set& s1, const const_stmt_set& s2) {
    
    const_stmt_set res;
    for (auto s1_e : s1) {
      bool found = false;
      for (auto s2_e : s2) {
	if (s1_e == s2_e) {
	  found = true;
	  break;
	}
      }
      if (!found) res.insert(s1_e);
    }
    return res;
  }
  
  // s1 U s2
  const_stmt_set set_union(const const_stmt_set& s1, const const_stmt_set& s2) {
    const_stmt_set res;
    for (auto s1_e : s1) {
      res.insert(s1_e);
    }
    for (auto s2_e : s2) {
      res.insert(s2_e);
    }
    return res;
  }
  
  defs_map def_union(defs_map& df1, defs_map& df2) {
    defs_map res;
    /*
    std::vector< const clang::Decl* > df1_keys, df2_keys;
    std::transform(df1.begin(), df1.end(), std::back_inserter(df1_keys),
		   [](const defs_map::value_type &pair){return pair.first;});
    std::transform(df2.begin(), df2.end(), std::back_inserter(df2_keys),
		   [](const defs_map::value_type &pair){return pair.first;});
    
    // k1 intersect k2, k1 - k2, k2 - k1
    std::vector< const clang::Decl* > common, only_df1, only_df2;
    std::set_intersection(df1_keys.begin(), df1_keys.end(), 
			  df2_keys.begin(), df2_keys.end(), 
			  std::back_inserter(common));
    std::set_difference(df1_keys.begin(), df1_keys.end(), 
			  df2_keys.begin(), df2_keys.end(), 
			  std::back_inserter(only_df1));
    std::set_difference(df2_keys.begin(), df2_keys.end(), 
			  df1_keys.begin(), df1_keys.end(), 
			  std::back_inserter(only_df2));
    
    for (auto decl : common) {
//       res[decl] = set_union(df1[decl], df2[decl]);
      std::set_union(df1[decl].begin(), df1[decl].end(), 
		     df2[decl].begin(), df2[decl].end(), 
		     std::inserter(res[decl], res[decl].begin()));
      if(const clang::VarDecl* vdecl = llvm::dyn_cast<clang::VarDecl>(decl)) {
	llvm::outs() << "common -------- " << vdecl->getNameAsString() << ": ";
	for (const Stmt* s : res[decl]) {
	  s->dumpPretty(*ast_ctx);
	  llvm::outs() << "    ";
	}
	llvm::outs() << "\n";
      }
    }
    for (auto decl : only_df1) {
      res[decl] = df1[decl];
    }
    for (auto decl : only_df2) {
      res[decl] = df2[decl];
    }*/
    
    for (auto e : df1) {
      auto decl = e.first;
      std::set_union(df1[decl].begin(), df1[decl].end(), 
		     df2[decl].begin(), df2[decl].end(), 
		     std::inserter(res[decl], res[decl].begin()));
    }
    for (auto e : df2) {
      auto decl = e.first;
      std::set_union(df1[decl].begin(), df1[decl].end(), 
		     df2[decl].begin(), df2[decl].end(), 
		     std::inserter(res[decl], res[decl].begin()));
    }
    
    return res;
  }
  
  defs_map def_diff(defs_map& df1, defs_map& df2) {
    defs_map res;
    
//     std::vector< const clang::Decl* > df1_keys, df2_keys;
//     auto l = [](const defs_map::value_type &pair){return pair.first;};
//     std::transform(df1.begin(), df1.end(), std::back_inserter(df1_keys), l);
//     std::transform(df2.begin(), df2.end(), std::back_inserter(df2_keys), l);
//     
//     // k1 intersect k2, k1 - k2, k2 - k1
//     std::vector< const clang::Decl* > common, only_df1;
//     std::set_intersection(df1_keys.begin(), df1_keys.end(), 
// 			  df2_keys.begin(), df2_keys.end(), 
// 			  std::back_inserter(common));
//     std::set_difference(df1_keys.begin(), df1_keys.end(), 
// 			  df2_keys.begin(), df2_keys.end(), 
// 			  std::back_inserter(only_df1));
//     
//     for (auto decl : common) {
// //       res[decl] = set_diff(df1[decl], df2[decl]);
//       std::set_difference(df1[decl].begin(), df1[decl].end(), 
// 			  df2[decl].begin(), df2[decl].end(), 
// 			  std::inserter(res[decl], res[decl].begin()));
//     }
//     for (auto decl : only_df1) {
//       res[decl] = df1[decl];
//     }
//     
    for (auto e : df1) {
      auto decl = e.first;
      std::set_difference(df1[decl].begin(), df1[decl].end(), 
			  df2[decl].begin(), df2[decl].end(), 
			  std::inserter(res[decl], res[decl].begin()));
    }
    
    return res;
  }
  
  void print_defs(defs_map& defs) {
    for (auto e : defs) {
      if(const clang::VarDecl* vdecl = llvm::dyn_cast<clang::VarDecl>(e.first)) {
	llvm::outs() << vdecl->getNameAsString() << ": ";
	for (const Stmt* s : e.second) {
	  s->dumpPretty(*ast_ctx);
	  llvm::outs() << "    ";
	}
	llvm::outs() << "\n";
      }
    }
  }
  
  void CalculateReachingDefs(const std::unique_ptr<clang::CFG>& fn_cfg) {
    using bb_defs_map = llvm::DenseMap< const clang::CFGBlock*, defs_map >;
    
    defs_map all_defs = CollectAllDefs(fn_cfg);
    
    stmt_varref_map uses = CollectUses(fn_cfg);
    /*
    llvm::outs() << "stmt n uses:\n";
    for (auto e : uses) {
      clang::LangOptions LangOpts;
      clang::PrintingPolicy Policy(LangOpts);
      const Stmt* stmt = e.first;
      stmt->printPretty(llvm::outs(), 0, Policy);
      llvm::outs() << ":\n";
      for (auto u : e.second) {
	u->printPretty(llvm::outs(), 0, Policy);
	llvm::outs() << " ";
      }
      llvm::outs() << "\n";
    }
    llvm::outs() << "\n";*/
    
    bb_defs_map reach_out;
    bb_defs_map kills, gens;
    
    // local calculations (each block) for gen and kill
    for (auto itr = fn_cfg->begin(); itr != fn_cfg->end(); ++itr) {
      const clang::CFGBlock* b = *itr;
      
      // calculate gen, kill
      defs_map g = CalculateGen(b);
      defs_map k = CalculateKill(b, all_defs);
      /*
      llvm::outs() << b->getBlockID() << ": \n";
      llvm::outs() << "~~~~~~~~~~gens~~~~~~~~~~~\n";
      for (auto e: g) {
	if(const clang::VarDecl* vdecl = llvm::dyn_cast<clang::VarDecl>(e.first)) {
	  llvm::outs() << vdecl->getNameAsString() << ": ";
	  for (const Stmt* s : e.second) {
	    s->dumpPretty(*ast_ctx);
	    llvm::outs() << " ";
	  }
	  llvm::outs() << "\n";
	}
      }
      llvm::outs() << "\n~~~~~~~~~~kills~~~~~~~~~~~\n";
      for (auto e: k) {
	if(const clang::VarDecl* vdecl = llvm::dyn_cast<clang::VarDecl>(e.first)) {
	  llvm::outs() << vdecl->getNameAsString() << ": ";
	  for (const Stmt* s : e.second) {
	    s->dumpPretty(*ast_ctx);
	    llvm::outs() << " ";
	  }
	  llvm::outs() << "\n";
	}
      }
      llvm::outs() << "--------------------\n\n";
      */
      kills[b] = k;
      gens[b] = g;
      
      reach_out[b] = defs_map();
    }
    
    // global calculations on the entier cfg
    bool changed = true;
    while(changed) {
      changed = false;
      
      for (auto itr = fn_cfg->rbegin(); itr != fn_cfg->rend(); ++itr) {
	const clang::CFGBlock* b = *itr;
	
	auto newreaches = reach_out[b];
	
	defs_map reach_in;
	for (auto p_itr = b->pred_begin(); p_itr != b->pred_end(); ++p_itr) {
	  const clang::CFGBlock* p = *p_itr;
	  for (auto d : reach_out[p]) {
	    const clang::Decl* def_decl = d.first;
	    for (const clang::Stmt* stmt: d.second) {
	      reach_in[def_decl].insert(stmt);
	    }
	  }
	}
	
	/*
	llvm::outs() << "processing " << b->getBlockID() << "...\n";
	llvm::outs() << "-------------reach_in-----------\n";
	print_defs(reach_in);
	llvm::outs() << "-------------kills-----------\n";
	print_defs(kills[b]);
	llvm::outs() << "-------------gens-----------\n";
	print_defs(gens[b]);*/
	
	// newreaches = gen U (reach_in - kill)
	auto diff = def_diff(reach_in, kills[b]);
	auto gen_u_diff = def_union(gens[b], diff);
	newreaches = def_union(newreaches, gen_u_diff);
	
	/*
	llvm::outs() << "-------------diff-----------\n";
	print_defs(diff);*/
	
	newreaches = def_union(gens[b], diff);
	
	/*
	llvm::outs() << "-------------newreaches-----------\n";
	print_defs(newreaches);*/
	
	
	
	if (!equals(newreaches, reach_out[b])) {
	  /*
	  llvm::outs() << b->getBlockID() << ":\n";
	  llvm::outs() << "~~~~~~~~~~~~~~~newreaches~~~~~~~~~~~\n";
	  for (auto e : newreaches) {
	    if(const clang::VarDecl* vdecl = llvm::dyn_cast<clang::VarDecl>(e.first)) {
	      llvm::outs() << vdecl->getNameAsString() << ": ";
	      for (const Stmt* s : e.second) {
		s->dumpPretty(*ast_ctx);
		llvm::outs() << "\n";
	      }
	      llvm::outs() << "\n";
	    }
	  }
	  llvm::outs() << "\n";
	  llvm::outs() << "~~~~~~~~~~~~~~~reaches~~~~~~~~~~~\n";
	  for (auto e : reaches[b]) {
	    if(const clang::VarDecl* vdecl = llvm::dyn_cast<clang::VarDecl>(e.first)) {
	      llvm::outs() << vdecl->getNameAsString() << ": ";
	      for (const Stmt* s : e.second) {
		s->dumpPretty(*ast_ctx);
		llvm::outs() << "\n";
	      }
	      llvm::outs() << "\n";
	    }
	  }
	  llvm::outs() << "\n";
	  */
	  reach_out[b] = newreaches;
	  changed = true;
	}
      }
      
    } // end while
    
    /*
    llvm::outs() << "\n\n~~~~~~~~~~~~~~~~~~reaches~~~~~~~~~~~~~~~~~~~\n";
    for (auto r_e : reaches) {
      llvm::outs() << r_e.first->getBlockID() << ":\n";
      llvm::outs() << "--------------------------------------------\n";
      for (auto e : r_e.second) {
	if(const clang::VarDecl* vdecl = llvm::dyn_cast<clang::VarDecl>(e.first)) {
	  llvm::outs() << vdecl->getNameAsString() << ": \n";
	  for (const Stmt* s : e.second) {
	    s->dumpPretty(*ast_ctx);
	    llvm::outs() << "\n";
	  }
	  llvm::outs() << "\n";
	}
      }
      llvm::outs() << "--------------------------------------------\n\n";
    }*/
    
    bb_defs_map reach_in;
    for (auto itr = fn_cfg->rbegin(); itr != fn_cfg->rend(); ++itr) {
      const clang::CFGBlock* bb = *itr;
      defs_map d_in;
      
      for (auto p_itr = bb->pred_begin(); p_itr != bb->pred_end(); ++p_itr) {
	const clang::CFGBlock* p = *p_itr;
	for (auto d : reach_out[p]) {
	  const clang::Decl* def_decl = d.first;
	  std::copy(d.second.begin(), d.second.end(), std::inserter(d_in[def_decl], d_in[def_decl].begin()));
	}
      }
      
      reach_in[bb] = d_in;
    }
    
    /*
    llvm::outs() << "\n\n~~~~~~~~~~~~~~~~~~reach_in~~~~~~~~~~~~~~~~~~~\n";
    for (auto r_e : reach_in) {
      llvm::outs() << r_e.first->getBlockID() << ":\n";
      llvm::outs() << "--------------------------------------------\n";
      for (auto e : r_e.second) {
	if(const clang::VarDecl* vdecl = llvm::dyn_cast<clang::VarDecl>(e.first)) {
	  llvm::outs() << vdecl->getNameAsString() << ": \n";
	  for (const Stmt* s : e.second) {
	    s->dumpPretty(*ast_ctx);
	    llvm::outs() << "\n";
	  }
	  llvm::outs() << "\n";
	}
      }
      llvm::outs() << "--------------------------------------------\n\n";
    }*/
    
    // build use-def chains, need to calculate defs that reach uses locally
    llvm::DenseMap< const clang::DeclRefExpr*, const_stmt_set > use_def_chain;
    for (auto itr = fn_cfg->rbegin(); itr != fn_cfg->rend(); ++itr) {
      const clang::CFGBlock* bb = *itr;
      // get reach_in
      defs_map r_in = reach_in[bb];
      
      // go over the block and build use_def_chain for each use
      for (const clang::CFGElement e : *bb) {
	switch(e.getKind()) {
	  case clang::CFGElement::Statement: {
	    
	    const Stmt *stmt = e.castAs<CFGStmt>().getStmt();
	    // for all uses in this stmt, update use_def_chain
	    for (auto u : uses[stmt]) {
	      use_def_chain[u] = r_in[u->getDecl()];
	    }
	    // if stmt is a def, remove all killed defs from r_in
	    // we determine if the statement is a def by searching for this statement in all_defs
	    for (auto e : all_defs) {
	      if (std::find(e.second.begin(), e.second.end(), stmt) != e.second.end()) {
		// stmt us a def!
		const_stmt_set s; s.insert(stmt);
		r_in[e.first] = s;
	      }
	    }
	    break;
	  }
	  default: llvm_unreachable("cfg element not handled...");
	}
      }
      
    }
    
//     SourceManager sm = TheRewriter.getSourceMgr();
    // print def use info
    for (auto itr = fn_cfg->rbegin(); itr != fn_cfg->rend(); ++itr) {
      const clang::CFGBlock* bb = *itr;
      llvm::outs() << "-----------------------BB#" << bb->getBlockID() << "-----------------------\n";
      for (const clang::CFGElement e : *bb) {
	if (const Stmt *stmt = e.castAs<CFGStmt>().getStmt()) {
	  auto stmt_line = TheRewriter.getSourceMgr().getPresumedLineNumber(stmt->getLocStart());
	  llvm::outs() << "stmt line#" << stmt_line << ": ";
	  for (auto u : uses[stmt]) {
	    for (auto def_stmt : use_def_chain[u]) {
	      auto def_stmt_line = TheRewriter.getSourceMgr().getPresumedLineNumber(def_stmt->getLocStart());
	      llvm::outs() << def_stmt_line << " ";
	    }
	  }
	  llvm::outs() << "\n";
	}
      }
      llvm::outs() << "----------------------------------------------\n\n";
    }
    
  }
  
  bool VisitFunctionDecl(clang::FunctionDecl* fdecl) {
    
    if (!fdecl->hasBody()) return true;
    if (!TheRewriter.getSourceMgr().isInMainFile(fdecl->getLocStart())) return true;
    
    /*
    // collect comm statements and associated free variables
    FVCollector fv_collect(TheRewriter, ast_ctx);
    fv_collect.TraverseDecl(fdecl);

    for (auto entry : fv_collect.stmt_fv_map) {
      llvm::outs() << entry.first->getLocStart().printToString(TheRewriter.getSourceMgr()) << "\n";
      for (auto fv : entry.second) {
	llvm::outs() << fv->getNameAsString() << " ";
      }
      llvm::outs() << "\n\n";
    }
    
    if (fv_collect.stmt_fv_map.empty()) {
      return true;
    }
    */
    // perform forward dataflow to check case
    std::unique_ptr<clang::CFG> fn_cfg = clang::CFG::buildCFG(fdecl, fdecl->getBody(), ast_ctx, clang::CFG::BuildOptions());
    
    CalculateReachingDefs(fn_cfg);
    
    return true;
  }
  
private:
  Rewriter &_rewriter;
  ASTContext* _ast_ctx;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class TestASTConsumer : public ASTConsumer {
public:
  TestASTConsumer(Rewriter &R, ASTContext* actx) : visitor(R, actx) {}

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
class TestFrontAction : public ASTFrontendAction {
public:
  TestFrontAction() {}
    
  void EndSourceFileAction() override {
    // Now emit the rewritten buffer.
//     TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID()).write(llvm::outs());
  }

  std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return llvm::make_unique<KanorASTConsumer>(TheRewriter, &(CI.getASTContext()));
  }

private:
  Rewriter _rewriter;
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
