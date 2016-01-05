#ifndef WIRECELLTBB_JOINCAT
#define WIRECELLTBB_JOINCAT

#include "WireCellIface/IJoinNode.h"
#include "WireCellTbb/NodeWrapper.h"

namespace WireCellTbb {


    /// A functional struct that when called will return the input
    /// ports of a tbb::flow::join_node as a vector of any receivers.
    template<class TupleType, int N>
    struct JoinNodeInputPorts {
	receiver_port_vector operator()(tbb::flow::join_node<TupleType>& jn) {
	    JoinNodeInputPorts<TupleType, N-1> next;
	    receiver_port_vector ret = next(jn);
	    receiver_type* rec = dynamic_cast<receiver_type*>(&tbb::flow::input_port<N-1>(jn));
	    ret.insert(ret.begin(), rec);
	    return ret;
	}
    };
    template<class TupleType>
    struct JoinNodeInputPorts<TupleType,0> {
	receiver_port_vector operator()(tbb::flow::join_node<TupleType>& jn) {
	    return receiver_port_vector();
	}
    };

	
    /// A functional struct that when called returns a tuple's values
    /// as an any_vector.
    template<class TupleType, int N>
    struct TupleValues {
	any_vector operator()(const TupleType& t) {
	    TupleValues<TupleType, N-1> next;
	    any_vector ret = next(t);
	    boost::any val = std::get<N-1>(t);
	    ret.insert(ret.begin(), val);
	    return ret;
	}
    };
    template<class TupleType>
    struct TupleValues<TupleType,0> {
	any_vector operator()(const TupleType& t) {
	    return any_vector();
	}
    };
    

    // /// A pure-code helper to access a tuple of compile-time size as a
    // /// vector of run-time size
    // template<class TupleType, int N>
    // struct TupleHelper {
    // 	TupleHelper<TupleType, N-1> nm1helper;

    // 	/// Break-out tuple of compile-time size into vector.
    // 	receiver_port_vector input_ports(tbb::flow::join_node<TupleType>& jn) {
    // 	    receiver_port_vector ret = nm1helper.input_ports(jn);
    // 	    receiver_type* rec = dynamic_cast<receiver_type*>(&tbb::flow::input_port<N-1>(jn));
    // 	    ret.insert(ret.begin(), rec);
    // 	    return ret;
    // 	}

    // 	/// Return tuple of compile-time size as vector of boost::any.
    // 	any_vector values(const TupleType& t) {
    // 	    any_vector ret = nm1helper.values(t);
    // 	    boost::any val = std::get<N-1>(t);
    // 	    ret.insert(ret.begin(), val);
    // 	    return ret;
    // 	}

    // };

    // /// Specialize to stop compile-time recursive template expansion
    // template<class TupleType>
    // struct TupleHelper<TupleType,0> {

    // 	receiver_port_vector input_ports(tbb::flow::join_node<TupleType>& jn) {
    // 	    return receiver_port_vector();
    // 	}
    // 	any_vector values(const TupleType& t) {

    // 	}
    // };


    //....

    // Body for a TBB join node.  It actually rides inside a function node
    template<typename TupleType, int N>    
    class JoinBody {
	WireCell::IJoinNodeBase::pointer m_wcnode;
    public:
	typedef typename WireCell::IJoinNodeBase::any_vector any_vector;

	JoinBody(WireCell::INode::pointer wcnode) {
	    m_wcnode = std::dynamic_pointer_cast<WireCell::IJoinNodeBase>(wcnode);
	}

	// fixme: check if this is the interface I really mean
	boost::any operator() (const TupleType &tup) const {
	    TupleValues<TupleType,N> values;
	    any_vector in = values(tup);
	    boost::any ret;
	    bool ok = (*m_wcnode)(in, ret);
	    return ret;
	}
	
    };

    template<typename TupleType, int N>
    receiver_port_vector build_joiner(tbb::flow::graph& graph, WireCell::INode::pointer wcnode,
				      tbb::flow::graph_node*& joiner, tbb::flow::graph_node*& caller)
    {
	// this node is fully TBB and joins N receiver ports into a tuple
	typedef tbb::flow::join_node< TupleType > tbb_join_node_type;
	tbb_join_node_type* jn = new tbb_join_node_type(graph);
	joiner = jn;

	// this node takes user WC body and runs it after converting input tuple to vector
	typedef tbb::flow::function_node<TupleType,boost::any> joining_node;
	joining_node* fn = new joining_node(graph, wcnode->concurrency(), JoinBody<TupleType,N>(wcnode));
	caller = fn;

	tbb::flow::make_edge(*jn, *fn);

	JoinNodeInputPorts<TupleType,N> ports;
	return ports(*jn);
    }
    
    // Wrap the TBB (compound) node
    class JoinWrapper : public NodeWrapper {
	tbb::flow::graph_node *m_joiner, *m_caller;
	receiver_port_vector m_receiver_ports;

    public:

	JoinWrapper(tbb::flow::graph& graph, WireCell::INode::pointer wcnode)
	    : m_joiner(0), m_caller(0)
	{
	    int nin = wcnode->input_types().size();
	    // an exhaustive switch to convert from run-time to compile-time types and enumerations.
	    switch (nin) {
	    case 1:
		m_receiver_ports = build_joiner<any_single, 1>(graph, wcnode, m_joiner, m_caller);
		break;
	    case 2:
		m_receiver_ports = build_joiner<any_double, 2>(graph, wcnode, m_joiner, m_caller);
		break;
	    case 3:
		m_receiver_ports = build_joiner<any_triple, 3>(graph, wcnode, m_joiner, m_caller);
		break;
	    default:
		// fixme: do something here
		break;
	    }
	    
	}
	
	virtual receiver_port_vector receiver_ports() {
	    return m_receiver_ports;
	}

	virtual sender_port_vector sender_ports() {
	    auto ptr = dynamic_cast< sender_type* >(m_caller);
	    return sender_port_vector{ptr};
	}

    };


}

#endif
