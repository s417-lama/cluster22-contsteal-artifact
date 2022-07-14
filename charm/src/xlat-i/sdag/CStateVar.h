#ifndef _CStateVar_H_
#define _CStateVar_H_

#include "xi-symbol.h"
#include "xi-util.h"

#include <list>

namespace xi {
class ParamList;

struct CStateVar {
  int isVoid;
  XStr* type;
  int numPtrs;
  XStr* name;
  XStr *byRef, *declaredRef;
  bool byConst;
  XStr* arrayLength;
  int isMsg;
  bool isRdma;
  bool isFirstRdma;
  bool isRecvRdma;
  bool isDevice;
  bool isFirstDeviceRdma;
  bool isCounter, isSpeculator;

  CStateVar(int v, const char* t, int np, const char* n, XStr* r, const char* a, int m);
  CStateVar(ParamList* pl);
};

struct EncapState {
  Entry* entry;
  XStr* type;
  XStr* name;
  bool isMessage;
  bool isForall;
  std::list<CStateVar*> vars;

  EncapState(Entry* entry, std::list<CStateVar*>& vars);
};
}  // namespace xi

#endif
