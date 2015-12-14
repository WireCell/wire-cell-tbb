// A minimally complete example of a tbb dfp

//#include "WireCellTbb/NodeWrapper.h"

#include "WireCellIface/IDepoSource.h"
#include "WireCellIface/IDrifter.h"
#include "WireCellIface/IDepoSink.h"
#include "WireCellIface/SimpleDepo.h"

#include "WireCellUtil/Testing.h"

#include <tbb/flow_graph.h>

#include <string>
#include <deque>
#include <iostream>
using namespace std;

class MockDepoSource : public WireCell::IDepoSource {
    int m_count;
    const int m_maxdepos;
public:
    MockDepoSource(int maxdepos = 10000) : m_count(0), m_maxdepos(maxdepos) {}
    virtual ~MockDepoSource() {}
    virtual bool extract(output_pointer& out) {
	//cerr << "Source: " << m_count << endl;
	if (m_count > m_maxdepos) {
	    cerr << "ModeDepoSource drained " << m_maxdepos << endl;
	    return false;
	}
	++m_count;
	double dist = m_count*units::millimeter;
	double time = m_count*units::microsecond;
	WireCell::Point pos(dist,dist,dist);
	out = WireCell::IDepo::pointer(new WireCell::SimpleDepo(time,pos));
	return true;
    }
};

class MockDrifter : public WireCell::IDrifter {
    std::deque<input_pointer> m_depos;
public:
    virtual ~MockDrifter() {}
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
    virtual ~MockDepoSink() {}
    virtual bool insert(const input_pointer& depo) {
	//cerr << "Depo sunk: " << depo->time() << " " << depo->pos() << endl;
    }    
};

// fixme: this fakes the factory until we clean up nodes to allow
// empty c'tor and use configuration.
WireCell::INode::pointer get_node(const std::string& node_desc)
{
    using namespace WireCell;

    if (node_desc == "source") { // note actual desc should be class or class:inst
	return INode::pointer(new MockDepoSource);
    }
    if (node_desc == "drift") { // note actual desc should be class or class:inst
	return INode::pointer(new MockDrifter);
    }
    if (node_desc == "sink") { // note actual desc should be class or class:inst
	return INode::pointer(new MockDepoSink);
    }
    return nullptr;
}




typedef tbb::flow::sender<boost::any>		sender_type;
typedef tbb::flow::receiver<boost::any>		receiver_type;
typedef std::vector<sender_type*>		sender_port_vector;
typedef std::vector<receiver_type*>		receiver_port_vector;

typedef tbb::flow::source_node<boost::any> source_node;
typedef tbb::flow::function_node<boost::any> sink_node;

typedef tbb::flow::tuple<boost::any> monotuple;
typedef tbb::flow::multifunction_node<boost::any, monotuple> queued_node;
typedef queued_node::output_ports_type queued_port;


// base facade, expose sender/receiver ports and provide initialize hook
class TbbNodeWrapper {
public:
    virtual ~TbbNodeWrapper() {}

    virtual sender_port_vector sender_ports() { return sender_port_vector(); }
    virtual receiver_port_vector receiver_ports() { return receiver_port_vector(); }
	
    // call before running graph
    virtual void initialize() { }

};

// expose wrappers only as a shared pointer
typedef std::shared_ptr<TbbNodeWrapper> TbbNode;


//
// SOURCE
//

// adapter to convert from WC source node to TBB source node body.
class TbbSourceBody {
    WireCell::ISourceNodeBase::pointer m_wcnode;
public:
    ~TbbSourceBody() {}

    TbbSourceBody(WireCell::INode::pointer wcnode) {
	m_wcnode = std::dynamic_pointer_cast<WireCell::ISourceNodeBase>(wcnode);
	Assert(m_wcnode);
    }
    bool operator()(boost::any& out) {
	//cerr << "Extracting from " << m_wcnode << endl;
	return m_wcnode->extract(out);
    }
};

// implement facade to access ports for source nodes
class TbbSourceNodeWrapper : public TbbNodeWrapper {
    source_node* m_tbbnode;
public:
    TbbSourceNodeWrapper(tbb::flow::graph& graph, WireCell::INode::pointer wcnode)
	: m_tbbnode(new source_node(graph, TbbSourceBody(wcnode), false))
	{    }
    ~TbbSourceNodeWrapper() {
	delete m_tbbnode;
    }
    virtual void initialize() {
	//cerr << "Activating source node\n";
	m_tbbnode->activate();
    }
    virtual sender_port_vector sender_ports() {
	auto ptr = dynamic_cast< sender_type* >(m_tbbnode);
	Assert(ptr);
	return sender_port_vector{ptr};
    }
};



//
// SINK
//

// adapter to convert from WC sink node to TBB sink node body.
class TbbSinkBody {
    WireCell::ISinkNodeBase::pointer m_wcnode;
public:
    ~TbbSinkBody() {}

    TbbSinkBody(WireCell::INode::pointer wcnode) {
	m_wcnode = std::dynamic_pointer_cast<WireCell::ISinkNodeBase>(wcnode);
	Assert(m_wcnode);
    }
    boost::any operator()(const boost::any& in) {
	//cerr << "Inserting to " << m_wcnode << endl;
	m_wcnode->insert(in);
	return in;
    }
};

// implement facade to access ports for sink nodes
class TbbSinkNodeWrapper : public TbbNodeWrapper {
    sink_node* m_tbbnode;
public:
    TbbSinkNodeWrapper(tbb::flow::graph& graph, WireCell::INode::pointer wcnode) :
	m_tbbnode(new sink_node(graph, wcnode->concurrency(), TbbSinkBody(wcnode))) { }
    ~TbbSinkNodeWrapper() {
	delete m_tbbnode;
    }

    virtual receiver_port_vector receiver_ports() {
	auto ptr = dynamic_cast< receiver_type* >(m_tbbnode);
	Assert(ptr);
	return receiver_port_vector{ptr};
    }
};


//
// FUNCTION
//

//
// MULTIOUT
//
class TbbMultioutBody {
    WireCell::IBufferNodeBase::pointer m_wcnode;
public:
    ~TbbMultioutBody() {}
    TbbMultioutBody(WireCell::INode::pointer wcnode) {
	m_wcnode = std::dynamic_pointer_cast<WireCell::IBufferNodeBase>(wcnode);
	Assert(m_wcnode);
    }
    void operator()(const boost::any& in, queued_port& out) {
	bool ok = m_wcnode->insert(in);
	while (true) {
	    boost::any anyout;
	    ok = m_wcnode->extract(anyout);
	    if (!ok) {
		break;
	    }
	    std::get<0>(out).try_put(anyout);
	}
    }
};
class TbbMultioutWrapper : public TbbNodeWrapper {
    queued_node* m_tbbnode;
public:
    TbbMultioutWrapper(tbb::flow::graph& graph, WireCell::INode::pointer wcnode)
	: m_tbbnode(new queued_node(graph, wcnode->concurrency(), TbbMultioutBody(wcnode)))
	{ }
    virtual ~TbbMultioutWrapper() {
	cerr << "TbbMultioutBody: Destructing\n";
	delete m_tbbnode;
    }
    
    virtual receiver_port_vector receiver_ports() {
	auto ptr = dynamic_cast< receiver_type* >(m_tbbnode);
	Assert(ptr);
	return receiver_port_vector{ptr};
    }

    virtual sender_port_vector sender_ports() {
	auto ptr = &tbb::flow::output_port<0>(*m_tbbnode);
	return sender_port_vector{ptr};
    }
};




// lookup WC node based on description, return wrapped node.
TbbNode make_node(tbb::flow::graph& graph, const std::string& node_desc)
{
    using namespace WireCell;

    INode::pointer wcnode = get_node(node_desc);
    if (! wcnode) {
	cerr << "Failed to get node for " << node_desc << endl; 
	return nullptr;
    }

    cerr << "Getting node from category: " << wcnode->category() << endl;
    switch (wcnode->category()) {
    case INode::sourceNode: 
	return TbbNode(new TbbSourceNodeWrapper(graph, wcnode));
    case INode::sinkNode:
    	return TbbNode(new TbbSinkNodeWrapper(graph, wcnode));
    // case INode::functionNode:
    // 	return TbbNode(new TbbFunctionWrapper(graph, wcnode));
    case INode::multioutNode:
    	return TbbNode(new TbbMultioutWrapper(graph, wcnode));
    default:
	return nullptr;
    }
    return nullptr;
}

bool connect(TbbNode sender, TbbNode receiver, int sport=0, int rport=0);
bool connect(TbbNode sender, TbbNode receiver, int sport, int rport)
{
    Assert(sender);
    Assert(receiver);
    auto sports = sender->sender_ports();
    auto rports = receiver->receiver_ports();

    Assert(sports.size() > sport);
    Assert(rports.size() > rport);
    
    sender_type* s = sports[sport];
    receiver_type* r = rports[rport];
    Assert(s);
    Assert(r);

    cerr << "Connecting " << s << " and " << r << endl;
    make_edge(*s, *r);
    return true;
}

int main()
{
    tbb::flow::graph graph;
    TbbNode source = make_node(graph, "source");
    Assert(source);
    TbbNode drift = make_node(graph, "drift");
    Assert(drift);
    TbbNode sink = make_node(graph, "sink");
    Assert(sink);

    Assert (connect(source, drift));
    Assert (connect(drift, sink));

    // fixme: in general all nodes should be initialize()'d but so far only source nodes need it.
    source->initialize();

    graph.wait_for_all();

    return 0;
}
