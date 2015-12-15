

#include "WireCellTbb/DataFlowGraph.h"

#include "WireCellUtil/Type.h"

#include <iostream>

using namespace std;
using namespace WireCell;
using namespace WireCellTbb;



DataFlowGraph::DataFlowGraph(int max_threads)
    : m_sched(max_threads > 0 ? max_threads : tbb::task_scheduler_init::automatic)
    , m_graph()
    , m_factory(m_graph)
{
}

DataFlowGraph::~DataFlowGraph()
{
}

bool DataFlowGraph::connect(INode::pointer tail, INode::pointer head,
			    int sport, int rport)
{
    using namespace WireCellTbb;

    Node mytail = m_factory(tail);
    if (!mytail) {
	cerr << "DFP: failed to get tail node wrapper\n";
	return false;
    }

    Node myhead = m_factory(head);
    if (!mytail) {
	cerr << "DFP: failed to get head node wrapper\n";
	return false;
    }

    auto sports = mytail->sender_ports();
    if (sport < 0 || sports.size() <= sport) {
	cerr << "DFP: bad sender port number: " << sport << endl;
	return false;
    }

    auto rports = myhead->receiver_ports();
    if (rport < 0 || rports.size() <= rport) {
	cerr << "DFP: bad receiver port number: " << rport << endl;
	return false;
    }

    
    sender_type* s = sports[sport];
    if (!s) {
	cerr << "DFP: failed to get sender port " << sport << endl;
	return false;
    }

    receiver_type* r = rports[rport];
    if (!s) {
	cerr << "DFP: failed to get receiver port " << rport << endl;
	return false;
    }


    cerr << "Connecting " << s << " and " << r << endl;
    make_edge(*s, *r);
    return true;
}
    

bool DataFlowGraph::run()
{
    for (auto it : m_factory.seen()) {
	it.second->initialize();
    }
    m_graph.wait_for_all();
    return true;
}

