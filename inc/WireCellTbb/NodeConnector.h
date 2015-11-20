#ifndef WIRECELLTBB_NODECONNECTOR
#define WIRECELLTBB_NODECONNECTOR

#include "WireCellUtil/Type.h"

#include <tbb/flow_graph.h>
#include <string>
#include <iostream>

namespace WireCellTbb {

    class INodeConnector {
    public:
	virtual ~INodeConnector(){}

	virtual std::string port_type_name() = 0;
	virtual bool connect(INodeWrapper* tail, INodeWrapper* head, int sport=0, int rport=0) = 0;
    };

    // A connector that can connect two nodes, one a sender and one a receiver, that pass the given data type.
    template<typename DataType>
    class NodeConnector : public INodeConnector {
    public:
	virtual ~NodeConnector() {}

	typedef DataType port_type;
	typedef std::shared_ptr<const DataType> data_pointer;
	typedef tbb::flow::sender<data_pointer> sender_type;
	typedef tbb::flow::receiver<data_pointer> receiver_type;
	

	std::string port_type_name() {
	    return typeid(port_type).name();
	}

	virtual bool connect(INodeWrapper* tail, INodeWrapper* head, int sport=0, int rport=0) {
	    auto sportv = tail->sender_ports();
	    auto rportv = head->receiver_ports();

	    if (sportv.empty()) {
		std::cerr << "DataFlowGraph: empty sender port vector from \""
			  << WireCell::demangle(tail->signature()) << "\"\n";
		return false;
	    }
	    if (rportv.empty()) {
		std::cerr << "DataFlowGraph: empty receiver port vector from \""
			  << WireCell::demangle(head->signature()) << "\"\n";
		return false;
	    }

	    IPortWrapper* sportw = sportv[sport];
	    IPortWrapper* rportw = rportv[rport];

	    if (!sportw) {
		std::cerr << "DataFlowGraph: failed to get sender port wrapper from \""
			  << WireCell::demangle(tail->signature()) << "\"\n";
		return false;
	    }

	    if (!rportw) {
		std::cerr << "DataFlowGraph: failed to get receiver port wrapper from \""
			  << WireCell::demangle(head->signature()) << "\"\n";
		return false;
	    }

	    auto stportw = dynamic_cast<SenderPortWrapper<DataType>*>(sportw);
	    auto rtportw = dynamic_cast<ReceiverPortWrapper<DataType>*>(rportw);

	    if (!stportw || !rtportw) {
		std::cerr << "DataFlowGraph: failed to cast port wrappers\n";
		return false;
	    }

	    auto s = stportw->sender();
	    auto r = rtportw->receiver();

	    if (!s) {
		std::cerr << "DataFlowGraph: failed to cast sender" << std::endl;
		std::cerr << "\tsender_type: \"" << WireCell::demangle(typeid(sender_type).name()) << "\"\n";
		return false;
	    }
	    if (!r) {
		std::cerr << "DataFlowGraph: failed to cast receiver" << std::endl;
		std::cerr << "\treceiver_type: \"" << WireCell::demangle(typeid(receiver_type).name()) << "\"\n";
		return false;
	    }

	    make_edge(*s, *r);
	    return true;
	}
    };

}


#endif
