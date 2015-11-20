#ifndef WIRECELLTBB_DATAFLOWGRAPH
#define WIRECELLTBB_DATAFLOWGRAPH

#include "WireCellIface/IDataFlowGraph.h"
#include "WireCellTbb/NodeMaker.h"
#include "WireCellTbb/NodeConnector.h"

#include <tbb/task_scheduler_init.h>

#include <map>
#include <string>

namespace WireCellTbb {

    class DataFlowGraph : public WireCell::IDataFlowGraph {
	tbb::task_scheduler_init m_sched;
	tbb::flow::graph m_graph;
	std::map<std::string, INodeMaker*> m_node_makers;
	std::map<std::string, INodeConnector*> m_node_connectors;
	std::map<WireCell::INode::pointer, INodeWrapper*> m_node_wrappers;

    public:
	DataFlowGraph(int max_threads = 0);
	virtual ~DataFlowGraph();

	/// Connect two nodes so that data runs from tail to head.
	/// Return false on error.
	virtual bool connect(WireCell::INode::pointer tail, WireCell::INode::pointer head,
			     int tail_port=0, int head_port=0);

	/// Run the graph, return false on error.
	virtual bool run();

	void add_maker(INodeMaker* maker);
	void add_connector(INodeConnector* connector);

    private:

	INodeWrapper* get_node_wrapper(WireCell::INode::pointer wcnode);
	INodeConnector* get_connector(const std::string& data_type_name);

    };

}

#endif

