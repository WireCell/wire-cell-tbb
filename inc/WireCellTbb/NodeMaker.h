#ifndef WIRECELLTBB_NODEMAKER
#define WIRECELLTBB_NODEMAKER

#include "WireCellIface/INode.h"
#include "WireCellTbb/NodeBody.h"

#include <tbb/flow_graph.h>

#include <string>
#include <vector>

namespace WireCellTbb {

    // Base class to something that knows how to make a TBB node of a certain signature
    class INodeMaker {
    public:
	virtual ~INodeMaker() {}
	virtual std::string signature() = 0;
	virtual tbb::flow::graph_node* make_node(tbb::flow::graph& graph, WireCell::INode::pointer wire_cell_node) = 0;

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

	virtual tbb::flow::graph_node* make_node(tbb::flow::graph& graph, WireCell::INode::pointer wc_inode) {
	    if (wc_inode->signature() != signature()) {
		return nullptr;
	    }
	    signature_pointer sig = std::dynamic_pointer_cast<signature_type>(wc_inode);
	    if (!sig) {
		return nullptr;
	    }
	    return new tbb::flow::source_node<output_pointer>(graph, SourceBody<signature_type>(sig), false);
	}

    };

};

#endif
