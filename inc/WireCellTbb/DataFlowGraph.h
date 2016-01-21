#ifndef WIRECELLTBB_DATAFLOWGRAPH
#define WIRECELLTBB_DATAFLOWGRAPH

#include "WireCellIface/IDataFlowGraph.h"
#include "WireCellIface/IConfigurable.h"
#include "WireCellTbb/NodeWrapper.h"
#include "WireCellTbb/WrapperFactory.h"

#include <tbb/task_scheduler_init.h>

#include <map>
#include <string>

namespace WireCellTbb {

    class DataFlowGraph : public WireCell::IDataFlowGraph, public WireCell::IConfigurable {
    public:
	DataFlowGraph(int max_threads = 0);
	virtual ~DataFlowGraph();

	/// Connect two nodes so that data runs from tail to head.
	/// Return false on error.
	virtual bool connect(WireCell::INode::pointer tail, WireCell::INode::pointer head,
			     int tail_port=0, int head_port=0);

	/// Run the graph, return false on error.
	virtual bool run();


	virtual void configure(const WireCell::Configuration& config);
	virtual WireCell::Configuration default_configuration() const;

    private:
	tbb::task_scheduler_init m_sched; // pass in number of threads
	tbb::flow::graph m_graph;	  // here lives the TBB graph
	WrapperFactory m_factory;
    };

}

#endif

