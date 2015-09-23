#include "Analysis/cfg_dump.h"

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

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