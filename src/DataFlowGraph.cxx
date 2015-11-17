#include "WireCellTbb/DataFlowGraph.h"

#include "WireCellIface/IDepoSource.h"
#include "WireCellIface/IDrifter.h"
#include "WireCellIface/IDiffuser.h"
#include "WireCellIface/IPlaneDuctor.h"

#include "WireCellUtil/Type.h"

#include <iostream>

using namespace std;
using namespace WireCell;
using namespace WireCellTbb;

DataFlowGraph::DataFlowGraph()
{
    // fixme: must add one of these lines for each INode interface.
    // It is here that we bind node behavior and node signature.
    // Behavior is set knowing the type of node interface (ISourceNode, etc).
    // Maybe there is some way to make this done covariantly?
    add_maker(new SourceNodeMaker<IDepoSource>);
    add_maker(new BufferNodeMaker<IDrifter>);
    add_maker(new BufferNodeMaker<IDiffuser>);
    add_maker(new BufferNodeMaker<IPlaneDuctor>);


    // fixme: must add one of these lines for each IData interface
    add_connector(new NodeConnector<IDepo>);
    add_connector(new NodeConnector<IDiffusion>);
    add_connector(new NodeConnector<IPlaneSlice>);
}

bool DataFlowGraph::connect(INode::pointer tail, INode::pointer head)
{
    auto outs = tail->output_types();
    auto ins = head->input_types();

    // for now just support 1-to-1.  In future add search for matching
    // port type names or allow for explicit port numbers.

    if (1 != outs.size()) {
	cerr << "DataFlowGraph: failed to connect tail with " << outs.size() << " output ports\n";
	return false;
    } 
    if (1 != ins.size()) { 
	cerr << "DataFlowGraph: failed to connect head with " << ins.size() << " input ports\n";
	return false; 
    } 
    if (outs[0] != ins[0]) {
	cerr << "DataFlowGraph: port type mismatch: \"" << demangle(outs[0]) << "\" != \"" << demangle(ins[0]) << "\"\n";
	return false;
    }

    auto conn = get_connector(ins[0]);
    if (!conn) {
	cerr << "DataFlowGraph: no connector for port type \"" << demangle(ins[0]) << "\"\n";
	return false;
    }

    auto s = get_node_wrapper(tail);
    auto r = get_node_wrapper(head);

    if (!s) {
	cerr << "DataFlowGraph: failed to get tail node for \"" << demangle(outs[0]) << "\"\n";
	return false;
    }
    if (!r) {
	cerr << "DataFlowGraph: failed to get head node for \"" << demangle(ins[0]) << "\"\n";
	return false;
    }

    bool ok = conn->connect(s,r);
    if (!ok) {
	cerr << "DataFlowGraph: failed to connect tail node: \"" << demangle(tail->signature()) << "\" to head node: \"" << demangle(head->signature()) << "\" along port of type \"" << demangle(ins[0]) << "\"\n";
    }
    cerr << "DataFlowGraph: connected tail node: \"" << demangle(tail->signature()) << "\" to head node: \"" << demangle(head->signature()) << "\" along port of type \"" << demangle(ins[0]) << "\"\n";

    return ok;
}


bool DataFlowGraph::run()
{
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
