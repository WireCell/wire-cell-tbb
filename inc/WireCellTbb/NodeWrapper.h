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
	typedef tbb::flow::sender<std::shared_ptr<const DataType> > sender_type;

	SenderPortWrapper(sender_type* sender) : m_sender(sender) {}

	virtual std::string port_type_name() { return typeid(DataType).name(); }
	sender_type* sender() { return m_sender; }
    private:
	sender_type* m_sender;
    };

    template<typename DataType>
    class ReceiverPortWrapper : public IPortWrapper {
    public:
	typedef tbb::flow::receiver<std::shared_ptr<const DataType> > receiver_type;

	ReceiverPortWrapper(receiver_type* receiver) : m_receiver(receiver) {}

	virtual std::string port_type_name() { return typeid(DataType).name(); }
	receiver_type* receiver() { return m_receiver; }

    private:
	receiver_type* m_receiver;

    };

    //
    // node wrapper - holds input/output port wrappers
    //

    class NodeWrapper {
    public:
	typedef std::vector<IPortWrapper*> port_vector;

	NodeWrapper() {}
	NodeWrapper(const port_vector& in, const port_vector& out) : m_in(in), m_out(out) {}
	
	//virtual std::string signature() { return typeid(void).name(); }

	virtual port_vector sender_ports() {
	    return m_out;
	}
	virtual port_vector receiver_ports() {
	    return m_in;
	}
    private:
	port_vector m_in;
	port_vector m_out;

    };



    class INodeWrapper {
    public:
	virtual ~INodeWrapper() {}
	//virtual tbb::flow::graph_node* tbb_node() = 0;
	virtual std::string signature() = 0;

	typedef std::vector<IPortWrapper*> port_vector;
	virtual port_vector sender_ports() { return port_vector(); }
	virtual port_vector receiver_ports() { return port_vector(); }
	
	// call before running graph
	virtual void initialize() { }

    };

    template<typename Signature>
    class SourceNodeWrapper : public INodeWrapper {
	tbb::flow::graph_node* m_node;
	IPortWrapper* m_port_wrapper; // just one for a source
    public:

	typedef tbb::flow::source_node<typename Signature::output_pointer> source_node_type;
	typedef tbb::flow::sender<typename Signature::output_pointer> sender_type;

	SourceNodeWrapper(tbb::flow::graph_node* tbb_node)
	    : m_node(tbb_node)
	{
	    auto s = dynamic_cast< sender_type* >(m_node);
	    m_port_wrapper = new SenderPortWrapper<typename Signature::output_type>(s);
	    Assert(m_port_wrapper);
	}

	//virtual tbb::flow::graph_node* tbb_node() { return m_node; }
	virtual std::string signature() { return typeid(Signature).name(); }

	virtual port_vector sender_ports() {
	    return port_vector{m_port_wrapper};
	}
	
	virtual void initialize() {
	    auto s = dynamic_cast< source_node_type* >(m_node);
	    s->activate();
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

	//virtual tbb::flow::graph_node* tbb_node() { return m_node; }
	virtual std::string signature() { return typeid(Signature).name(); }

	virtual port_vector sender_ports() {
	    return port_vector{m_sender};
	}
	virtual port_vector receiver_ports() {
	    return port_vector{m_receiver};
	}
    };
    

    class GeneralNodeWrapper : public INodeWrapper {
	port_vector m_in;
	port_vector m_out;
    public:
	GeneralNodeWrapper(const port_vector& in, const port_vector& out) : m_in(in), m_out(out) {}
	
	virtual std::string signature() { return typeid(void).name(); }

	virtual port_vector sender_ports() {
	    return m_out;
	}
	virtual port_vector receiver_ports() {
	    return m_in;
	}

    };
}

#endif

