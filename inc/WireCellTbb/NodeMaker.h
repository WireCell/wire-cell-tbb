#ifndef WIRECELLTBB_NODEMAKER
#define WIRECELLTBB_NODEMAKER

#include "WireCellIface/INode.h"
#include "WireCellTbb/NodeBody.h"
#include "WireCellTbb/NodeWrapper.h"

#include <tbb/flow_graph.h>

#include <string>
#include <vector>

namespace WireCellTbb {

    // Base class to something that knows how to make a TBB node of a certain signature
    class INodeMaker {
    public:
	virtual ~INodeMaker() {}
	virtual std::string signature() = 0;
	virtual INodeWrapper* make_node_wrapper(tbb::flow::graph& graph, WireCell::INode::pointer wire_cell_node) = 0;

    };

    // Maker of TBB source nodes holding a WireCell::ISourceNode.
    template<typename Signature>
    class SourceNodeMaker : public INodeMaker {
    public:
	typedef Signature signature_type;
	typedef std::shared_ptr<Signature> signature_pointer;
	typedef typename Signature::output_type output_type;
	typedef typename Signature::output_pointer output_pointer;

	virtual ~SourceNodeMaker() {}

	virtual std::string signature() {
	    return typeid(signature_type).name();
	}

	virtual INodeWrapper* make_node_wrapper(tbb::flow::graph& graph, WireCell::INode::pointer wc_inode) {
	    if (wc_inode->signature() != signature()) {
		return nullptr;
	    }
	    signature_pointer sig = std::dynamic_pointer_cast<signature_type>(wc_inode);
	    if (!sig) {
		return nullptr;
	    }
	    auto node = new tbb::flow::source_node<output_pointer>(graph, SourceBody<signature_type>(sig), false);
	    return new SourceNodeWrapper<Signature>(node);
	}

    };

    // Maker of TBB function nodes holding a WireCell::IFunctionNode
    template<typename Signature>
    class FunctionNodeMaker : public INodeMaker {
    public:
	typedef Signature signature_type;
	typedef std::shared_ptr<Signature> signature_pointer;
	typedef typename Signature::output_type output_type;
	typedef typename Signature::output_pointer output_pointer;
	typedef typename Signature::input_type input_type;
	typedef typename Signature::input_pointer input_pointer;
	
	typedef tbb::flow::receiver< input_pointer > receiver_type;
	typedef tbb::flow::sender< output_pointer > sender_type;

	virtual ~FunctionNodeMaker() {}

	virtual std::string signature() {
	    return typeid(signature_type).name();
	}

	virtual INodeWrapper* make_node_wrapper(tbb::flow::graph& graph, WireCell::INode::pointer wc_inode) {
	    if (wc_inode->signature() != signature()) {
		return nullptr;
	    }
	    signature_pointer sig = std::dynamic_pointer_cast<signature_type>(wc_inode);
	    if (!sig) {
		return nullptr;
	    }

	    auto fnode = new tbb::flow::function_node<input_pointer, output_pointer>(graph, wc_inode->concurrency(),
										     FunctionBody<signature_type>(sig));
	    auto node = dynamic_cast<tbb::flow::graph_node*>(fnode);

	    receiver_type* receiver = dynamic_cast<receiver_type*>(node);
	    INodeWrapper::port_vector rv = {
		new ReceiverPortWrapper<input_type>(receiver)
	    };
	    sender_type* sender = dynamic_cast<sender_type*>(node);
	    INodeWrapper::port_vector sv = {
		new SenderPortWrapper<output_type>(sender)
	    };
	    INodeWrapper* ret = new GeneralNodeWrapper(rv, sv);
	    return ret;
	}
    };


    // Maker of TBB multifunction nodes holding a WireCell::IBufferNode
    template<typename Signature>
    class BufferNodeMaker : public INodeMaker {
    public:
	typedef Signature signature_type;
	typedef std::shared_ptr<Signature> signature_pointer;
	typedef typename Signature::output_type output_type;
	typedef typename Signature::output_pointer output_pointer;
	typedef typename Signature::input_type input_type;
	typedef typename Signature::input_pointer input_pointer;
	
	typedef tbb::flow::multifunction_node<input_pointer, tbb::flow::tuple<output_pointer> > tbbmf_node;
	typedef typename tbbmf_node::output_ports_type output_ports_type;


	virtual ~BufferNodeMaker() {}

	virtual std::string signature() {
	    return typeid(signature_type).name();
	}

	virtual INodeWrapper* make_node_wrapper(tbb::flow::graph& graph, WireCell::INode::pointer wc_inode) {
	    if (wc_inode->signature() != signature()) {
		return nullptr;
	    }
	    signature_pointer sig = std::dynamic_pointer_cast<signature_type>(wc_inode);
	    if (!sig) {
		return nullptr;
	    }

	    auto mf_node = new tbbmf_node(graph, wc_inode->concurrency(), BufferBody<signature_type>(sig));
	    auto node = dynamic_cast<tbb::flow::graph_node*>(mf_node);
	    INodeWrapper* ret = new FunctionNodeWrapper<Signature>(node);
	    return ret;
	}
    };

    template<typename Signature>
    class JoinNodeMaker : public INodeMaker {
    public:
	typedef Signature signature_type;
	typedef std::shared_ptr<Signature> signature_pointer;
	typedef typename Signature::output_type output_type;
	typedef typename Signature::output_pointer output_pointer;
	typedef typename Signature::input_type input_type;
	typedef typename Signature::input_pointer input_pointer;

	virtual ~JoinNodeMaker() {}

	virtual std::string signature() {
	    return typeid(signature_type).name();
	}

	virtual INodeWrapper* make_node_wrapper(tbb::flow::graph& graph, WireCell::INode::pointer wc_inode) {
	    if (wc_inode->signature() != signature()) {
		return nullptr;
	    }
	    signature_pointer sig = std::dynamic_pointer_cast<signature_type>(wc_inode);
	    if (!sig) {
		return nullptr;
	    }

	    // sig is IJoinNode
	    // make a tbb::flow::join_node
	    if (sig->ninputs() == 3) {
		typedef tbb::flow::tuple<input_pointer,input_pointer,input_pointer> tuple_type;
		auto jn = new tbb::flow::join_node<tuple_type>(graph);
		auto jb = JoinBody3<signature_type>(sig);

		typedef tbb::flow::multifunction_node<tuple_type, tbb::flow::tuple<output_pointer> > tbbmf_node;
		typedef typename tbbmf_node::output_ports_type output_ports_type;
		auto fn = new tbbmf_node(graph, 1, jb);

		// internal edge
		make_edge(*jn, *fn);

		INodeWrapper::port_vector rv = {
		    new ReceiverPortWrapper<input_type>(&tbb::flow::input_port<0>(*jn)),
		    new ReceiverPortWrapper<input_type>(&tbb::flow::input_port<1>(*jn)),
		    new ReceiverPortWrapper<input_type>(&tbb::flow::input_port<2>(*jn))
		};
		
		typedef tbb::flow::sender<output_pointer> sender_type;
		//sender_type* sender = dynamic_cast< sender_type* >(&tbb::flow::output_port<0>(*fn));
		sender_type* sender = &tbb::flow::output_port<0>(*fn);
		INodeWrapper::port_vector sv;
		sv.push_back(new SenderPortWrapper<output_type>(sender));
		
                INodeWrapper* ret = new GeneralNodeWrapper(rv, sv);
		return ret;
		// we leak the tbb nodes, so sorry.
	    }
	    return nullptr;	// repeat above for different number of args
	}
	
	
    };


}

#endif
