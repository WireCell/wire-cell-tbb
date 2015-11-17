#ifndef WIRECELLTBB_NODEWRAPPER
#define WIRECELLTBB_NODEWRAPPER

#include "WireCellUtil/Testing.h"

#include <tbb/flow_graph.h>

#include <string>
#include <iostream>


namespace WireCellTbb {

    //
    // port wrapper
    //

    class IPortWrapper {
    public:
	virtual std::string port_type_name() = 0;
    };

    template<typename DataType>
    class SenderPortWrapper : public IPortWrapper {
    public:
	typedef tbb::flow::sender<typename DataType::pointer> sender_type;

	SenderPortWrapper(sender_type* sender) : m_sender(sender) {}

	virtual std::string port_type_name() { return typeid(DataType).name(); }
	sender_type* sender() { return m_sender; }
    private:
	sender_type* m_sender;
    };

    template<typename DataType>
    class ReceiverPortWrapper : public IPortWrapper {
    public:
	typedef tbb::flow::receiver<typename DataType::pointer> receiver_type;

	ReceiverPortWrapper(receiver_type* receiver) : m_receiver(receiver) {}

	virtual std::string port_type_name() { return typeid(DataType).name(); }
	receiver_type* receiver() { return m_receiver; }

    private:
	receiver_type* m_receiver;

    };
    

    //
    // node wrapper
    //

    class INodeWrapper {
    public:
	virtual ~INodeWrapper() {}
	virtual tbb::flow::graph_node* tbb_node() = 0;
	virtual std::string signature() = 0;
	virtual IPortWrapper* sender_port(int port=0) { return nullptr; }
	virtual IPortWrapper* receiver_port(int port=0) { return nullptr; }
    };

    template<typename Signature>
    class SourceNodeWrapper : public INodeWrapper {
	tbb::flow::graph_node* m_node;
	IPortWrapper* m_port_wrapper; // just one for a source
    public:

	typedef tbb::flow::sender<typename Signature::output_pointer> sender_type;

	SourceNodeWrapper(tbb::flow::graph_node* tbb_node)
	    : m_node(tbb_node)
	{
	    auto s = dynamic_cast< sender_type* >(tbb_node);
	    m_port_wrapper = new SenderPortWrapper<typename Signature::output_type>(s);
	    Assert(m_port_wrapper);
	}

	virtual tbb::flow::graph_node* tbb_node() { return m_node; }
	virtual std::string signature() { return typeid(Signature).name(); }

	virtual IPortWrapper* sender_port(int port=0) {
	    if (port != 0) { return nullptr; }
	    return m_port_wrapper;
	}
	
    };
    

    // one in, one out
    template<typename Signature>
    class FunctionNodeWrapper : public INodeWrapper {
	tbb::flow::graph_node* m_node;
	IPortWrapper* m_receiver;
	IPortWrapper* m_sender;
	
    public:
	typedef tbb::flow::multifunction_node<typename Signature::input_pointer,
					      tbb::flow::tuple<typename Signature::output_pointer> > tbbmf_node;

	typedef tbb::flow::sender<typename Signature::output_pointer> sender_type;
	typedef tbb::flow::receiver<typename Signature::input_pointer> receiver_type;

	FunctionNodeWrapper(tbb::flow::graph_node* tbb_node)
	    : m_node(tbb_node)
	{
	    receiver_type* r = dynamic_cast<receiver_type*>(tbb_node);
	    m_receiver = new ReceiverPortWrapper<typename Signature::input_type>(r);

	    tbbmf_node* mf_node = dynamic_cast<tbbmf_node*>(tbb_node);
	    sender_type* s = &tbb::flow::output_port<0>(*mf_node); // only first port
	    m_sender = new SenderPortWrapper<typename Signature::output_type>(s);
	}

	virtual tbb::flow::graph_node* tbb_node() { return m_node; }
	virtual std::string signature() { return typeid(Signature).name(); }

	virtual IPortWrapper* sender_port(int port=0) {
	    if (port != 0) { return nullptr; }
	    return m_sender;
	}
	virtual IPortWrapper* receiver_port(int port=0) {
	    if (port != 0) { return nullptr; }
	    return m_receiver;
	}
    };
    

}

#endif

