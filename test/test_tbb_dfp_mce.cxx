// A minimally complete example of a tbb dfp

#include <tbb/flow_graph.hpp>
#include <boost/any.hpp>
#include <memory>
#include <string>
#include <ordered_map>

#include "WireCellIface/IDepoSource.h"
#include "WireCellIface/IDrifter.h"
#include "WireCellIface/IDepoSink.h"


class MockDepoSource : public WireCell::IDepoSource {
    int m_count;
    const m_maxdepos;
public:
    MockDepoSource(int maxdepos = 10) : m_count(0), m_maxdepos(maxdepos) {}
    virtual bool extract(output_pointer& out) {
	if (m_count > m_maxdepos) {
	    return false;
	}
	++m_count;
	out = WireCell::IDepo::pointer(new SimpleDepo);
	return true;
    }
};

class MockDrifter : public WireCell::IDrifter {
    std::deque m_depos;
public:
    virtual bool insert(const input_pointer& depo) {
	m_depos.push_back(depo);
	return true;
    }
    virtual bool extract(output_pointer& depo) {
	if (m_depos.empty()) {
	    return false;
	}
	depo = m_depos.front();
	m_depos.pop_front();
	return true;
    }
};

class MockDepoSink : public WireCell::IDepoSink {
public:
    virtual bool insert(const IDepo::pointer& depo) {
	cerr << "Depo sunk: " << depo->time() << " " << depo->pos() << endl;
    }    
};

// fixme: this fakes the factory until we clean up nodes to allow
// empty c'tor and use configuration.
WireCell::INode::pointer get_node(const std::string& node_desc)
{
    using namespace WireCell;

    if (node_desc == "source") { // note actual desc should be class or class:inst
	return INode::pointer(new MockDepoSource());
    }
    if (node_desc == "drift") { // note actual desc should be class or class:inst
	return INode::pointer(new MockDrifter());
    }
    if (node_desc == "sink") { // note actual desc should be class or class:inst
	return INode::pointer(new MockDepoSink());
    }
    return nullptr;
}

NodeWrapper wrap_source(tbb::flow::graph& graph, WireCell::INode::pointer wcnode)
{
    
}

NodeWrapper make_node_wrapper(tbb::flow::graph& graph, const std::string& node_desc)
{
    using namespace WireCell;

    INode::pointer wcnode = get_node(node_desc);
    if (! wcnode) {
	return NodeWrapper();
    }

    switch (wcnode->category()) {
    case INode::sourceNode: 
	return wrap_source(wcnode);
    case INode::sinkNode:
	return wrap_sink(wcnode);
    case INode::functionNode:
	return wrap_function(wcnode);
    default:
	return NodeWrapper();
    }
}


