#ifndef WIRECELLTBB_NODEBODY
#define WIRECELLTBB_NODEBODY

#include "WireCellIface/INode.h"

#include <tbb/flow_graph.h>

namespace WireCellTbb {

    // fixme:move into NodeBody.h
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


};

#endif
