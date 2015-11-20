#include "WireCellTbb/DataFlowGraph.h"

#include "WireCellIface/IDepoSource.h"
#include "WireCellIface/IDrifter.h"
#include "WireCellIface/IDiffuser.h"
#include "WireCellIface/IPlaneDuctor.h"
#include "WireCellIface/IPlaneSliceMerger.h"

#include "WireCellUtil/Type.h"

#include <iostream>

using namespace std;
using namespace WireCell;
using namespace WireCellTbb;

DataFlowGraph::DataFlowGraph(int max_threads)
    : m_sched(max_threads > 0 ? max_threads : tbb::task_scheduler_init::automatic)
    , m_graph()
{
    // fixme: must add one of these lines for each INode interface.
    // It is here that we bind node behavior and node signature.
    // Behavior is set knowing the type of node interface (ISourceNode, etc).
    // Maybe there is some way to make this done covariantly?
    add_maker(new SourceNodeMaker<IDepoSource>);
    add_maker(new BufferNodeMaker<IDrifter>);
    add_maker(new BufferNodeMaker<IDiffuser>);
    add_maker(new BufferNodeMaker<IPlaneDuctor>);
    add_maker(new JoinNodeMaker<IPlaneSliceMerger>);


    // fixme: must add one of these lines for each IData interface
    add_connector(new NodeConnector<IDepo>);
    add_connector(new NodeConnector<IDiffusion>);
    add_connector(new NodeConnector<IPlaneSlice>);
    add_connector(new NodeConnector<IPlaneSlice::vector>);
}

DataFlowGraph::~DataFlowGraph()
{
}

bool DataFlowGraph::connect(INode::pointer tail, INode::pointer head,
			    int sport, int rport)
{
    auto outs = tail->output_types();
    auto ins = head->input_types();

    if (0 > sport || sport >= outs.size()) {
	cerr << "DataFlowGraph: failed to connect tail with " << outs.size() << " output ports\n";
	return false;
    } 
    if (0 > rport || rport >= ins.size()) { 
	cerr << "DataFlowGraph: failed to connect head with " << ins.size() << " input ports\n";
	return false; 
    } 

    auto conn = get_connector(ins[rport]);
    if (!conn) {
	cerr << "DataFlowGraph: no connector for port type \"" << demangle(ins[rport]) << "\"\n";
	return false;
    }

    auto s = get_node_wrapper(tail);
    auto r = get_node_wrapper(head);

    if (!s) {
	cerr << "DataFlowGraph: failed to get tail node for \"" << demangle(outs[sport]) << "\"\n";
	return false;
    }
    if (!r) {
	cerr << "DataFlowGraph: failed to get head node for \"" << demangle(ins[rport]) << "\"\n";
	return false;
    }

    bool ok = conn->connect(s,r,sport,rport);
    if (!ok) {
	cerr << "DataFlowGraph: failed to connect tail node: \""
	     << demangle(tail->signature())
	     << "\" to head node: \"" << demangle(head->signature())
	     << "\" along port of type \"" << demangle(ins[rport]) << "\"\n";
    }
    cerr << "DataFlowGraph: connected tail node: \""
	 << demangle(tail->signature())
	 << "\" to head node: \"" << demangle(head->signature())
	 << "\" along port of type \"" << demangle(ins[rport]) << "\"\n";

    return ok;
}


bool DataFlowGraph::run()
{
    for (auto it : m_node_wrappers) {
	it.second->initialize();
    }
    m_graph.wait_for_all();
    return true;
}


void DataFlowGraph::add_maker(INodeMaker* maker)
{
    cerr << "DataFlowGraph: adding maker for: \"" << demangle(maker->signature()) << "\"\n";
    m_node_makers[maker->signature()] = maker;
}
void DataFlowGraph::add_connector(INodeConnector* connector)
{
    cerr << "DataFlowGraph: adding connector for: \"" << demangle(connector->port_type_name()) << "\"\n";
    m_node_connectors[connector->port_type_name()] = connector;
}

INodeWrapper* DataFlowGraph::get_node_wrapper(WireCell::INode::pointer wcnode)
{
    auto nit = m_node_wrappers.find(wcnode);
    if (nit != m_node_wrappers.end()) {
	return nit->second;
    }

    auto mit = m_node_makers.find(wcnode->signature());
    if (mit == m_node_makers.end()) {
	cerr << "DataFlowGraph: failed to get node maker with signature: \"" << demangle(wcnode->signature()) << "\"\n";
	return nullptr;
    }

    auto tbb_node_wrapper = mit->second->make_node_wrapper(m_graph, wcnode);
    if (!tbb_node_wrapper) { 
	cerr << "DataFlowGraph: failed to make node wrapper with signature: \"" << demangle(wcnode->signature()) << "\"\n";
	return nullptr; 
    }

    cerr << "DataFlowGraph: made node wrapper with signature: \"" << demangle(wcnode->signature()) << "\"\n";

    m_node_wrappers[wcnode] = tbb_node_wrapper;
    return tbb_node_wrapper;
}


INodeConnector* DataFlowGraph::get_connector(const std::string& data_type_name)
{
    auto cit = m_node_connectors.find(data_type_name);
    if (cit == m_node_connectors.end()) {
	cerr << "DataFlowGraph: failed to get connector for type: \"" << demangle(data_type_name) << "\"\n";
	return nullptr;
    }
    return cit->second;
}
