//===-- clang/Analysis/DefUse.h - DefUse analysis -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of DefUse analysis headers
//
//===----------------------------------------------------------------------===//

#ifndef DEF_USE_HPP_
#define DEF_USE_HPP_

#include "llvm/ADT/DenseMap.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ParentMap.h"
#include "clang/AST/StmtVisitor.h"

#include <exception>
#include <stdexcept>

namespace clang {

// forward definitions
class CFG;
class CFGBlock;

namespace defuse {

//===------------------------- Exceptions -------------------------===//

struct NotDefExcpetion {
    Stmt const* stmt;
    NotDefExcpetion(Stmt const* stmt_): stmt(stmt_){}
};
struct NotUseExcpetion {
    DeclRefExpr const* expr;
    NotUseExcpetion(DeclRefExpr const* expr_): expr(expr_){}
};

//===------------------------- DefUseNode -------------------------===//
// Keeps the information for a particular 'definition' or 'use' of a variable
// The structure is needed because a defuse chain can contains variable declarations
// as well as variable reference. As a DeclStmt can contains several declarations
// to address a particular variable we need to store its VarDecl or DeclRefExp
struct DefUseNode{
	enum NodeKind{ VarDecl, VarRef };
	enum UseKind{ Use, Def, UseDef };

	DefUseNode(clang::VarDecl const* decl): var_decl(decl), kind(VarDecl), usage(Def){}
	DefUseNode(DeclRefExpr const* ref, UseKind u = Use): var_ref(ref), kind(VarRef), usage(u){}

	clang::VarDecl const* getDecl() const;

	NodeKind const& getKind()const { return kind; }
	UseKind const&  getUse() const { return usage; }

	clang::VarDecl const* getVarDecl() const {
		assert(kind==VarDecl); return var_decl;
	}
	DeclRefExpr const* getVarRef() const {
		assert(kind==VarRef); return var_ref;
	}

	bool operator==(DefUseNode const& n) const;

private:
	// a def-use node can be either a VarDecl or a DeclRefExpr
	union{
		clang::VarDecl	const* 	var_decl;
		DeclRefExpr const* 		var_ref;
	};
	NodeKind 	kind;
	UseKind 	usage;
};

//===------------------------- Typedefs -------------------------===//
class DefUseBlock;
class VarDeclMap;
typedef std::vector<DefUseBlock>				DefUseData;
typedef llvm::DenseMap<Stmt const*, unsigned> 	VarRefBlockMap;
typedef std::vector<DefUseNode>					DefUseVect;
typedef std::vector<DefUseNode const*> 			VarRefsVect;

class DefUseBasicHelper;

//===------------------------- DefUse -------------------------===//

class DefUse{
	ASTContext const&		ctx;
	DefUseData const* 		analysis_data;
	VarDeclMap const*		decl_map;
	VarRefBlockMap const* 	block_map;
	unsigned const			num_cfg_blocks;

	DefUse(ASTContext const& ctx_, DefUseData const* analysis_data_, VarDeclMap const* decl_map_,
	        VarRefBlockMap const* block_map_, unsigned num_blocks):
		ctx(ctx_), analysis_data(analysis_data_), decl_map(decl_map_), block_map(block_map_),
		num_cfg_blocks(num_blocks){ }

	bool isDef(DefUseNode const& n) const;

	struct iterator_impl{
		DefUse const* 		du;
		DefUseNode const*	node;

		struct iter{
			int		 					block_id;
			DefUseVect::const_iterator 	block_it;
			iter(int blk_id): block_id(blk_id){}
		};

		iterator_impl(): du(NULL){ }
		iterator_impl(DefUse const* du_): du(du_){ }
		iterator_impl& operator++() { return inc(false); }
		iterator_impl& operator++(int) { return inc(false); }
		virtual iterator_impl& inc(bool) = 0;
	};
public:
	class uses_iterator: public std::iterator<std::input_iterator_tag, DeclRefExpr, std::ptrdiff_t, DeclRefExpr const*>,
                         public iterator_impl{
		iter iter_ptr;
		bool inDefBlock;

		uses_iterator(): iterator_impl(), iter_ptr(-1), inDefBlock(true){}
		uses_iterator(DefUse const* du, DefUseNode const& n);
		uses_iterator& inc(bool first);
		friend class DefUse;
	public:
		virtual bool operator!=(uses_iterator const& iter);
		virtual DeclRefExpr const* operator*();
	};

	class defs_iterator: public std::iterator<std::input_iterator_tag, DefUseNode, std::ptrdiff_t, DefUseNode const*>,
                         public iterator_impl{
		struct iter_: public iter{
			VarRefsVect::const_iterator 	reaches_it;
			iter_(int blk_id): iter(blk_id){}
		} iter_ptr;
		bool blockDef;

		defs_iterator(): iterator_impl(), iter_ptr(-1), blockDef(false) {}
		defs_iterator(DefUse const* du, DeclRefExpr const& n);
		defs_iterator& inc(bool first);
		friend class DefUse;
	public:
		bool operator!=(defs_iterator const& iter);
		DefUseNode const* operator*();
	};

	// USES //
	defs_iterator defs_begin(DeclRefExpr const* var) const;
	defs_iterator defs_end() const;

	// DEFS //
	uses_iterator uses_begin(DeclRefExpr const* var) const;
	uses_iterator uses_begin(VarDecl const* var) const;
	uses_iterator uses_end() const;

	bool isUse(DeclRefExpr const* var) const;
	bool isDef(DeclRefExpr const* var) const { return isDef(DefUseNode(var, DefUseNode::Def)); }
	bool isDef(VarDecl const* var) const { return isDef(DefUseNode(var)); }

	~DefUse();

	static DefUse* BuildDefUseChains(Stmt* body, ASTContext *ctx, DefUseBasicHelper* helper = 0,
										CFG* cfg = 0, ParentMap* pm = 0, bool verbose = false,
										llvm::raw_ostream& out = llvm::outs());
};

//===------------------------- DefUseBasicHelper -------------------------===//

class DefUseBasicHelper: public StmtVisitor<DefUseBasicHelper>{
	class DefUseBasicHelperImpl;
	DefUseBasicHelperImpl* 	pimpl;

	void InitializeValues(DefUseData* data, VarRefBlockMap* bm, VarDeclMap* dm);

	friend class DefUse;
public:
	DefUseNode::UseKind current_use;
	DefUseBasicHelper();

	virtual void HandleDeclRefExpr(DeclRefExpr *DR); // remember to call the super class implementation of the method
	void HandleDeclStmt(DeclStmt *DS);

	virtual void HandleBinaryOperator(BinaryOperator* B);
	virtual void HandleConditionalOperator(ConditionalOperator* C);
	virtual void HandleCallExpr(CallExpr* C);
	virtual void HandleUnaryOperator(UnaryOperator* U);
	virtual void HandleArraySubscriptExpr(ArraySubscriptExpr* AS);
	// ...

	void VisitDeclRefExpr(DeclRefExpr *DR){ return HandleDeclRefExpr(DR); }
	void VisitDeclStmt(DeclStmt *DS){ return HandleDeclStmt(DS); }
	void VisitBinaryOperator(BinaryOperator* B){ return HandleBinaryOperator(B); }
	void VisitConditionalOperator(ConditionalOperator* C){ return HandleConditionalOperator(C); }
	void VisitCallExpr(CallExpr* C){ return HandleCallExpr(C); }
	void VisitUnaryOperator(UnaryOperator* U){ return HandleUnaryOperator(U); }
	void VisitArraySubscriptExpr(ArraySubscriptExpr* AS){ return HandleArraySubscriptExpr(AS); }
	// ...

	void VisitCFGBlock(clang::CFGBlock const& blk, int root);
	void VisitStmt(Stmt* S);

	virtual ~DefUseBasicHelper();
};

//===------------------------- DefUseChainTest -------------------------===//

class DefUseChainTest: public StmtVisitor<DefUseChainTest>, public ASTConsumer {
	llvm::raw_ostream&		out;
	ASTContext* 			ctx;
	DefUse const*			du;

public:
	DefUseChainTest(llvm::raw_ostream& out_): out(out_), ctx(NULL), du(NULL) {	}

	void Initialize(ASTContext& context) { ctx = &context; }
	void HandleTopLevelDecl(DeclGroupRef declGroupRef);
	// void HandleTranslationUnit(clang::ASTContext& context);

	void VisitDeclRefExpr(DeclRefExpr *DR);
	void VisitDeclStmt(DeclStmt *DS);
	void VisitStmt(Stmt* S);
};

void PrintVarDefs(DefUse const* DU, DeclRefExpr const* DR, ASTContext& ctx, llvm::raw_ostream& out);
void PrintVarUses(DefUse const* DU, DeclRefExpr const* DR, ASTContext& ctx, llvm::raw_ostream& out);
void PrintVarUses(DefUse const* DU, VarDecl const* VD, ASTContext& ctx, llvm::raw_ostream& out);

} // end namespace defuse
} // end namespace clang

#endif /* DEF_USE_HPP_ */
