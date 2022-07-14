/**
 * \file MetisLB.h
 */

/**
 * \addtogroup CkLdb
 */
/*@{*/

#ifndef _METISLB_H_
#define _METISLB_H_

#include "CentralLB.h"
#include "MetisLB.decl.h"

void CreateMetisLB();
BaseLB* AllocateMetisLB();

class MetisLB : public CBase_MetisLB
{
public:
  MetisLB(const CkLBOptions&);
  MetisLB(CkMigrateMessage* m) : CBase_MetisLB(m) { lbname = "MetisLB"; }

private:
  bool QueryBalanceNow(int step) override { return true; }
  void work(LDStats* stats) override;
};

#endif /* _METISLB_H_ */

/*@}*/
