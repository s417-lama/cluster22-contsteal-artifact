#include "main.decl.h"

#include "searchEngine.h"
#include "uts.h"

int initial_grainsize;

class Main : public CBase_Main
{
public:
    Main(CkArgMsg* msg )
    {
        uts_parseParams(msg->argc - 2, &msg->argv[2]);
        initial_grainsize = atoi(msg->argv[1]);                 
        int nrepeats = atoi(msg->argv[2]);
        delete msg;
        uts_printParams();                                                                                
        searchEngineProxy.start(nrepeats);
    }
};

#include "main.def.h"
