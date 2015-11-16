#include "WireCellTbb/DataFlowGraph.h"

#include "WireCellIface/IDepoSource.h"


using namespace WireCell;
using namespace WireCellTbb;

DataFlowGraph::DataFlowGraph()
{
    // fixme: must add one of these lines for each INode interface
    add_maker(new SourceNodeMaker<IDepoSource>);

    // fixme: must add one of these lines for each IData interface
    add_connector(new NodeConnector<IDepo::pointer>);
}

bool DataFlowGraph::connect(INode::pointer tail, INode::pointer head)
{
    auto outs = tail->output_types();
    auto ins = head->input_types();

    // for now just support 1-to-1.  In future add search for matching
    // port type names or allow for explicit port numbers.

    if (1 != outs.size()) { return false; } 
    if (1 != ins.size()) { return false; } 
    if (outs[0] != ins[0]) { return false; }

    auto conn = get_connector(ins[0]);
    if (!conn) { return false; }

    auto s = get_tbb_node(tail);
    auto r = get_tbb_node(head);

    return conn->connect(s,r);
}


bool DataFlowGraph::run()
{
}


void DataFlowGraph::add_maker(INodeMaker* maker)
{
    m_node_makers[maker->signature()] = maker;
}
void DataFlowGraph::add_connector(INodeConnector* connector)
{
    m_node_connectors[connector->port_type_name()] = connector;
}

tbb::flow::graph_node* DataFlowGraph::get_tbb_node(WireCell::INode::pointer wcnode)
{
    auto nit = m_wc2tbb.find(wcnode);
    if (nit != m_wc2tbb.end()) {
	return nit->second;
    }

    auto mit = m_node_makers.find(wcnode->signature());
    if (mit == m_node_makers.end()) {
	return nullptr;
    }

    auto tbb_node = mit->second->make_node(m_graph, wcnode);
    if (!tbb_node) { return nullptr; }

    m_wc2tbb[wcnode] = tbb_node;
    return tbb_node;
}


INodeConnector* DataFlowGraph::get_connector(const std::string& data_type_name)
{
    auto cit = m_node_connectors.find(data_type_name);
    if (cit == m_node_connectors.end()) {
	return nullptr;
    }
    return cit->second;
}
