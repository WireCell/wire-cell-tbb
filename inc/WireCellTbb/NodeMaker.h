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

}

#endif
