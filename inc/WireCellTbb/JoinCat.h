#ifndef WIRECELLTBB_JOINCAT
#define WIRECELLTBB_JOINCAT

#include "WireCellIface/IJoinNode.h"
#include "WireCellTbb/NodeWrapper.h"

namespace WireCellTbb {

    // Body for a TBB join node.  It actually rides inside a function node
    class JoinBody {
	WireCell::IJoinNodeBase::pointer m_wcnode;
    public:
	typedef typename WireCell::IJoinNodeBase::vectorany vectorany;

	JoinBody(WireCell::INode::pointer wcnode) {
	    m_wcnode = std::dynamic_pointer_cast<WireCell::IJoinNodeBase>(wcnode);
	}

	// fixme: check if this is the interface I really mean
	boost::any operator() (const vectorany &v) const {
	    boost::any ret;
	    bool ok = (*m_wcnode)(v, ret);
	    return ret;
	}
	
    };

    // Wrap the TBB (compound) node
    class JoinWrapper : public NodeWrapper {
	tbb::flow::graph_node* m_joiner, *m_caller;
// fixme: still a WIP
    public:

	JoinWrapper(tbb::flow::graph& graph, WireCell::INode::pointer wcnode) {
	    int nin = wcnode->input_types().size();
	    
	}
	
	virtual receiver_port_vector receiver_ports() {
	    // fixme
	    //auto ptr = dynamic_cast< receiver_type* >(m_tbbnode);
	    //return receiver_port_vector{ptr};
	}

	virtual sender_port_vector sender_ports() {
	    // fixme
	    //auto ptr = &tbb::flow::output_port<0>(*m_tbbnode);
	    //return sender_port_vector{ptr};
	}


    };


}

#endif
