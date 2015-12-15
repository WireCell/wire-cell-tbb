#ifndef WIRECELLTBB_NODEWRAPPER
#define WIRECELLTBB_NODEWRAPPER

#include "WireCellIface/INode.h"

#include <tbb/flow_graph.h>
#include <boost/any.hpp>
#include <memory>
#include <map>

namespace WireCellTbb {

    // Broken out sender/receiver types, vectors of their pointers
    typedef tbb::flow::sender<boost::any>	sender_type;
    typedef tbb::flow::receiver<boost::any>	receiver_type;

    typedef std::vector<sender_type*>		sender_port_vector;
    typedef std::vector<receiver_type*>		receiver_port_vector;


    // A base facade which expose sender/receiver ports and provide
    // initialize hook.  There is one NodeWrapper for each node
    // category.
    class NodeWrapper {
    public:
	virtual ~NodeWrapper() {}
	
	virtual sender_port_vector sender_ports() { return sender_port_vector(); }
	virtual receiver_port_vector receiver_ports() { return receiver_port_vector(); }
	
	// call before running graph
	virtual void initialize() { }
	
    };

    // expose the wrappers only as a shared pointer
    typedef std::shared_ptr<NodeWrapper> Node;
}

#endif
