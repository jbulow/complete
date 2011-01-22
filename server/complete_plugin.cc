/*

g++ -c complete_plugin.cc \
    `~/src/llvm-svn/Release+Asserts/bin/llvm-config --cxxflags` \
    -I/Users/thakis/src/llvm-svn/tools/clang/include

g++ -dynamiclib -Wl,-undefined,dynamic_lookup \
    -lsqlite3 complete_plugin.o -o libcomplete_plugin.dylib

~/src/llvm-svn/Release+Asserts/bin/clang++ -c complete_plugin.cc\
    `~/src/llvm-svn/Release+Asserts/bin/llvm-config --cxxflags` \
    -I/Users/thakis/src/llvm-svn/tools/clang/include \
    -Xclang -load -Xclang libcomplete_plugin.dylib \
    -Xclang -add-plugin -Xclang complete

*/
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/AST.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
using namespace clang;

#include "sqlite3.h"

// TODO(thakis): Less hardcoded.
const char kDbPath[] = "/Users/thakis/src/chrome-git/src/builddb.sqlite";


class StupidDatabase {
public:
  StupidDatabase() : db_(NULL) {}
  ~StupidDatabase() { close(); }

  bool open(const std::string& file) {
    assert(!db_);
    int err = sqlite3_open(file.c_str(), &db_);
    return err == SQLITE_OK;
  }

  void close() {
    if (db_)
      sqlite3_close(db_);
    db_ = NULL;
  }
private:
  sqlite3* db_;
};


class CompletePlugin : public ASTConsumer {
public:
  CompletePlugin(CompilerInstance& instance)
      : instance_(instance),
        d_(instance.getDiagnostics()) {
    // FIXME: Emit diag instead?
    if (!db_.open(kDbPath))
      fprintf(stderr, "Failed to open db\n");
  }

  virtual void HandleTopLevelDecl(DeclGroupRef D);

private:
  CompilerInstance& instance_;
  Diagnostic& d_;
  StupidDatabase db_;
};

void CompletePlugin::HandleTopLevelDecl(DeclGroupRef D) {
  for (DeclGroupRef::iterator I = D.begin(), E = D.end(); I != E; ++I) {
    if (NamedDecl* record = dyn_cast<NamedDecl>(*I)) {
      SourceLocation record_location = record->getLocStart();
      SourceManager& source_manager = instance_.getSourceManager();

      record_location = source_manager.getInstantiationLoc(record_location);

      // Ignore built-ins.
      if (record_location.isInvalid()) return;

      // Ignore stuff from system headers.
      if (source_manager.isInSystemHeader(record_location)) return;

      // Ignore everything not in the main file.
      if (!source_manager.isFromMainFile(record_location)) return;

      //std::string identifier = record->getIdentifier();
      std::string identifier = record->getNameAsString();
      fprintf(stderr, "%s:%u Identifier: %s\n",
          source_manager.getBufferName(record_location),
          source_manager.getInstantiationLineNumber(record_location),
          identifier.c_str());
    }
  }
}


class CompletePluginAction : public PluginASTAction {
 protected:
  ASTConsumer* CreateASTConsumer(CompilerInstance &CI, llvm::StringRef ref) {
    return new CompletePlugin(CI);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string>& args) {
    return true;
  }
};


static FrontendPluginRegistry::Add<CompletePluginAction>
X("complete", "Adds information about symbols to a sqlite database.");
