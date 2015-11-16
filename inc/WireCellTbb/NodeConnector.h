#ifndef WIRECELLTBB_NODECONNECTOR
#define WIRECELLTBB_NODECONNECTOR

#include <tbb/flow_graph.h>
#include <string>

namespace WireCellTbb {

    class INodeConnector {
    public:
	virtual ~INodeConnector(){}

	virtual std::string port_type_name() = 0;
	virtual bool connect(tbb::flow::graph_node* tail, tbb::flow::graph_node* head) = 0;
    };

    // A connector that can connect two nodes, one a sender and one a receiver, that pass the given data type.
    template<typename DataType>
    class NodeConnector : public INodeConnector {
    public:
	virtual ~NodeConnector() {}

	typedef DataType port_type;
	typedef tbb::flow::sender<DataType> sender_type;
	typedef tbb::flow::receiver<DataType> receiver_type;
	

	std::string port_type_name() {
	    return typeid(port_type).name();
	}

	virtual bool connect(tbb::flow::graph_node* tail, tbb::flow::graph_node* head) {
	    auto s = dynamic_cast<sender_type*>(tail);
	    auto r = dynamic_cast<receiver_type*>(head);

	    if (!s || !r) { return false; }

	    make_edge(*s, *r);
	    return true;
	}
    };

}


#endif
