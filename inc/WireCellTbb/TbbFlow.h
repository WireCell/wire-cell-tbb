#ifndef WIRECELLTBB_TBBFLOW
#define WIRECELLTBB_TBBFLOW

#include "WireCellIface/IApplication.h"
#include "WireCellIface/IConfigurable.h"
#include "WireCellIface/IDataFlowGraph.h"

namespace WireCellTbb {

    class TbbFlow : public WireCell::IApplication, public WireCell::IConfigurable {
	WireCell::IDataFlowGraph::pointer m_dfp;
    public:
	TbbFlow();
	virtual ~TbbFlow();

	virtual void configure(const WireCell::Configuration& config);
	virtual WireCell::Configuration default_configuration() const;

	virtual void execute();
    };
}

#endif
