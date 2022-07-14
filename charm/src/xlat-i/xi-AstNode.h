#ifndef _AST_NODE_H
#define _AST_NODE_H

#include "xi-util.h"
#include <algorithm>
#include <list>

namespace xi {

class AstNode : public Printable {
 protected:
  int line;

 public:
  explicit AstNode(int line_ = -1) : line(line_) {}
  virtual void outputClosuresDecl(XStr& str) { (void)str; }
  virtual void outputClosuresDef(XStr& str) { (void)str; }
  virtual void genDecls(XStr& str) { (void)str; }
  virtual void genDefs(XStr& str) { (void)str; }
  virtual void genClosureEntryDecls(XStr& str) {}
  virtual void genClosureEntryDefs(XStr& str) {}
  virtual void genReg(XStr& str) { (void)str; }
  virtual void genGlobalCode(XStr scope, XStr& decls, XStr& defs) {
    (void)scope;
    (void)decls;
    (void)defs;
  }
  virtual void preprocess() {}
  virtual void check() {}
  virtual void printChareNames() {}

  virtual void genTramTypes() {}
  virtual void genTramRegs(XStr& str) { (void)str; }
  virtual void genTramPups(XStr& scope, XStr& decls, XStr& defs) {
    (void)scope;
    (void)decls;
    (void)defs;
  }
};

template <typename Child>
class AstChildren : public virtual AstNode {
 protected:
  std::list<Child*> children;

 public:
  AstChildren(int line_, Child* c, AstChildren* cs) : AstNode(line_) {
    children.push_back(c);
    if (cs) children.insert(children.end(), cs->children.begin(), cs->children.end());
  }

  template <typename T>
  explicit AstChildren(std::list<T*>& l) {
    children.insert(children.begin(), l.begin(), l.end());
    l.clear();
  }
  void push_back(Child* c);

  void preprocess();
  void check();
  void print(XStr& str);

  void printChareNames();

  void outputClosuresDecl(XStr& str);
  void outputClosuresDef(XStr& str);

  void genClosureEntryDecls(XStr& str);
  void genClosureEntryDefs(XStr& str);
  void genDecls(XStr& str);
  void genDefs(XStr& str);
  void genReg(XStr& str);
  void genGlobalCode(XStr scope, XStr& decls, XStr& defs);

  bool isTramTarget();
  void genTramTypes();
  void genTramRegs(XStr& str);
  void genTramPups(XStr& scope, XStr& decls, XStr& defs);

  template <typename T>
  void recurse(T arg, void (Child::*fn)(T));
  void recursev(void (Child::*fn)());
};

namespace details {
using std::list;
using std::for_each;

/**
   Apply fn_ on each Construct in the list l, passing it arg as
   the target. If between_ is passed, do that to arg between each
   element.
*/
template <typename T, typename U, typename A>
class perElemGenC {
  void (U::*fn)(A);
  void (*between)(A);
  A arg;

 public:
  perElemGenC(list<T*>& l, A arg_, void (U::*fn_)(A), void (*between_)(A) = NULL)
      : fn(fn_), between(between_), arg(arg_) {
    for_each(l.begin(), l.end(), *this);
  }
  void operator()(T* m) {
    if (m) {
      (m->*fn)(arg);
      if (between) between(arg);
    }
  }
};

template <typename T, typename U, typename A>
void perElemGen(list<T*>& l, A& arg_, void (U::*fn_)(A&),
                // Sun Studio 7 (C++ compiler version 5.4) can't handle this
                //              void (*between_)(A&) = NULL)
                void (*between_)(A&)) {
  perElemGenC<T, U, A&>(l, arg_, fn_, between_);
}

template <typename T, typename U, typename A>
void perElemGen(list<T*>& l, A& arg_, void (U::*fn_)(A&)) {
  perElemGenC<T, U, A&>(l, arg_, fn_, NULL);
}

template <typename T, typename U, typename A>
void perElemGen(list<T*>& l, A* arg_, void (U::*fn_)(A*),
                // See above
                //              void (*between_)(A*) = NULL)
                void (*between_)(A*)) {
  perElemGenC<T, U, A*>(l, arg_, fn_, between_);
}

template <typename T, typename U, typename A>
void perElemGen(list<T*>& l, A* arg_, void (U::*fn_)(A*)) {
  perElemGenC<T, U, A*>(l, arg_, fn_, NULL);
}

/**
   Apply fn_ on each Construct in the list l, passing it arg as
   the target. If between_ is passed, do that to arg between each
   element.
*/
template <typename T, typename U, typename A>
class perElemGen2C {
  void (U::*fn)(A, A);
  void (*between)(A, A);
  A arg1, arg2;

 public:
  perElemGen2C(list<T*>& l, A arg1_, A arg2_, void (U::*fn_)(A, A),
               void (*between_)(A, A) = NULL)
      : fn(fn_), between(between_), arg1(arg1_), arg2(arg2_) {
    for_each(l.begin(), l.end(), *this);
  }
  void operator()(T* m) {
    if (m) {
      (m->*fn)(arg1, arg2);
      if (between) between(arg1, arg2);
    }
  }
};

template <typename T, typename U, typename A>
void perElemGen2(list<T*>& l, A& arg1_, A& arg2_, void (U::*fn_)(A&, A&),
                 // Sun Studio 7 (C++ compiler version 5.4) can't handle this
                 //              void (*between_)(A&) = NULL)
                 void (*between_)(A&, A&)) {
  perElemGen2C<T, U, A&>(l, arg1_, arg2_, fn_, between_);
}

template <typename T, typename U, typename A>
void perElemGen2(list<T*>& l, A& arg1_, A& arg2_, void (U::*fn_)(A&, A&)) {
  perElemGen2C<T, U, A&>(l, arg1_, arg2_, fn_, NULL);
}

template <typename T, typename U, typename A>
void perElemGen2(list<T*>& l, A* arg1_, A* arg2_, void (U::*fn_)(A*, A*),
                 // See above
                 //              void (*between_)(A*) = NULL)
                 void (*between_)(A*, A*)) {
  perElemGen2C<T, U, A*>(l, arg1_, arg2_, fn_, between_);
}

template <typename T, typename U, typename A>
void perElemGen2(list<T*>& l, A* arg1_, A* arg2_, void (U::*fn_)(A*, A*)) {
  perElemGen2C<T, U, A*>(l, arg1_, arg2_, fn_, NULL);
}

/**
   Apply fn_ on each non-NULL element in the list l.
   If between_ is passed, do that between each element.
*/
template <typename T, typename U>
class perElemC {
  void (U::*fn)();

 public:
  perElemC(list<T*>& l, void (U::*fn_)()) : fn(fn_) {
    for_each(l.begin(), l.end(), *this);
  }
  void operator()(T* m) {
    if (m) {
      (m->*fn)();
    }
  }
};

template <typename T, typename U>
void perElem(list<T*>& l, void (U::*fn_)()) {
  perElemC<T, U>(l, fn_);
}

void newLine(XStr& str);
}  // namespace details

template <typename Child>
void AstChildren<Child>::push_back(Child* m) {
  children.push_back(m);
}

template <typename Child>
void AstChildren<Child>::print(XStr& str) {
  details::perElemGen(children, str, &Child::print);
}

template <typename Child>
void AstChildren<Child>::preprocess() {
  details::perElem(children, &Child::preprocess);
}

template <typename Child>
void AstChildren<Child>::check() {
  details::perElem(children, &Child::check);
}

template <typename Child>
void AstChildren<Child>::genDecls(XStr& str) {
  details::perElemGen(children, str, &Child::genDecls, details::newLine);
}

template <typename Child>
void AstChildren<Child>::genClosureEntryDecls(XStr& str) {
  details::perElemGen(children, str, &Child::genClosureEntryDecls, details::newLine);
}

template <typename Child>
void AstChildren<Child>::genClosureEntryDefs(XStr& str) {
  details::perElemGen(children, str, &Child::genClosureEntryDefs, details::newLine);
}

template <typename Child>
void AstChildren<Child>::outputClosuresDecl(XStr& str) {
  details::perElemGen(children, str, &Child::outputClosuresDecl, details::newLine);
}

template <typename Child>
void AstChildren<Child>::outputClosuresDef(XStr& str) {
  details::perElemGen(children, str, &Child::outputClosuresDef, details::newLine);
}

template <typename Child>
void AstChildren<Child>::genDefs(XStr& str) {
  details::perElemGen(children, str, &Child::genDefs, details::newLine);
}

template <typename Child>
void AstChildren<Child>::genReg(XStr& str) {
  details::perElemGen(children, str, &Child::genReg, details::newLine);
}

template <typename Child>
template <typename T>
void AstChildren<Child>::recurse(T t, void (Child::*fn)(T)) {
  details::perElemGen(children, t, fn);
}

template <typename Child>
void AstChildren<Child>::recursev(void (Child::*fn)()) {
  details::perElem(children, fn);
}

template <typename Child>
void AstChildren<Child>::genGlobalCode(XStr scope, XStr& decls, XStr& defs) {
  for (typename std::list<Child*>::iterator i = children.begin(); i != children.end();
       ++i) {
    if (*i) {
      (*i)->genGlobalCode(scope, decls, defs);
    }
  }
}

template <typename Child>
void AstChildren<Child>::printChareNames() {
  details::perElem(children, &Child::printChareNames);
}

template <typename Child>
bool AstChildren<Child>::isTramTarget() {
  for (typename std::list<Child*>::iterator i = children.begin(); i != children.end();
       ++i)
    if ((*i)->isTramTarget()) return true;

  return false;
}

template <typename Child>
void AstChildren<Child>::genTramTypes() {
  details::perElem(children, &Child::genTramTypes);
}

template <typename Child>
void AstChildren<Child>::genTramRegs(XStr& str) {
  details::perElemGen(children, str, &Child::genTramRegs);
}

template <typename Child>
void AstChildren<Child>::genTramPups(XStr& scope, XStr& decls, XStr& defs) {
  for (typename std::list<Child*>::iterator i = children.begin(); i != children.end();
       ++i) {
    if (*i) {
      (*i)->genTramPups(scope, decls, defs);
    }
  }
}

}  // namespace xi

#endif  // ifndef _AST_NODE_H
