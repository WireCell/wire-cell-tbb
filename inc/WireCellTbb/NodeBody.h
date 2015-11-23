#ifndef WIRECELLTBB_NODEBODY
#define WIRECELLTBB_NODEBODY

#include "WireCellIface/INode.h"

#include <tbb/flow_graph.h>

namespace WireCellTbb {

    template<typename Signature>
    class SourceBody {
    public:
	typedef Signature signature_type;
	typedef std::shared_ptr<Signature> signature_pointer;
	typedef typename Signature::output_type output_type;
	typedef typename Signature::output_pointer output_pointer;

	SourceBody(signature_pointer sig) : m_signature(sig) {}

	// copy c'tor - should this clone the underlying WC node to allow for proper concurrency?
	SourceBody(const SourceBody& other) : m_signature(other.m_signature) {}
	
	// assignment - should this clone the underlying WC node to allow for proper concurrency?
	void operator=( const SourceBody& other) { m_signature = other.m_signature; }

	~SourceBody() {}


	bool operator()(output_pointer& out) {
	    return m_signature->extract(out);
	}
    private:
	signature_pointer m_signature;

    };


    template<typename Signature>
    class BufferBody {
    public:
	typedef Signature signature_type;
	typedef std::shared_ptr<Signature> signature_pointer;
	typedef typename Signature::output_type output_type;
	typedef typename Signature::output_pointer output_pointer;
	typedef typename Signature::input_type input_type;
	typedef typename Signature::input_pointer input_pointer;

	typedef tbb::flow::multifunction_node<input_pointer, tbb::flow::tuple<output_pointer> > tbbmf_node;
	typedef typename tbbmf_node::output_ports_type output_ports_type;

	BufferBody(signature_pointer sig) : m_signature(sig) {}

	void operator()(const input_pointer& in, output_ports_type& out) {
	    bool ok = m_signature->insert(in);

	    while (true) {
		output_pointer o;
		ok = m_signature->extract(o);
		if (!ok) {
		    break;
		}

		/// fixme: this only handles a single output port.
		std::get<0>(out).try_put(o); // handle errors? we don't need to handle no stinking errors.
	    }
	}	
    private:
	signature_pointer m_signature;
    };

    template<typename Signature>
    class FunctionBody {
    public:
	typedef Signature signature_type;
	typedef std::shared_ptr<Signature> signature_pointer;
	typedef typename Signature::output_type output_type;
	typedef typename Signature::output_pointer output_pointer;
	typedef typename Signature::input_type input_type;
	typedef typename Signature::input_pointer input_pointer;

	FunctionBody(signature_pointer sig) : m_signature(sig) {}

	output_pointer operator()(const input_pointer& in) {
	    output_pointer out;
	    bool ok = (*m_signature)(in, out);
	    return out;
	}
    private:
	signature_pointer m_signature;
    };

    template<typename Signature>
    class JoinBody3 {
    public:
	typedef Signature signature_type;
	typedef std::shared_ptr<Signature> signature_pointer;
	typedef typename Signature::output_type output_type;
	typedef typename Signature::output_pointer output_pointer;
	typedef typename Signature::input_type input_type;
	typedef typename Signature::input_pointer input_pointer;

	typedef tbb::flow::tuple<input_pointer,input_pointer,input_pointer> tuple_type;

	typedef tbb::flow::multifunction_node<input_pointer, tbb::flow::tuple<output_pointer> > tbbmf_node;
	typedef typename tbbmf_node::output_ports_type output_ports_type;

	JoinBody3(signature_pointer sig) : m_signature(sig) {}

	void operator()(const tuple_type& in, output_ports_type& out) {
	    
	    bool ok =
		m_signature->insert(std::get<0>(in),0) &&
		m_signature->insert(std::get<1>(in),1) &&
		m_signature->insert(std::get<2>(in),2);

	    while (true) {
		output_pointer o;
		ok = m_signature->extract(o);
		if (!ok) {
		    break;
		}

		/// fixme: this only handles a single output port.
		std::get<0>(out).try_put(o); // handle errors? we don't need to handle no stinking errors.
	    }
	}	
    private:
	signature_pointer m_signature;
    };
    

    template<typename Signature>
    class SinkBody {
    public:
	typedef Signature signature_type;
	typedef std::shared_ptr<Signature> signature_pointer;
	typedef typename Signature::input_type input_type;
	typedef typename Signature::input_pointer input_pointer;

	SinkBody(signature_pointer sig) : m_signature(sig) {}

	input_pointer operator()(const input_pointer& in) {
	    bool ok = m_signature->insert(in);
	    return in;
	}
    private:
	signature_pointer m_signature;
    };

};

#endif
