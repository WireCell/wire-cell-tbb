#include "WireCellIface/IWireParameters.h"
#include "WireCellIface/IWireGenerator.h"
#include "WireCellIface/SimpleNodes.h"

#include "WireCellIface/ICell.h"
#include "WireCellIface/ICellMaker.h"


#include "WireCellUtil/PluginManager.h"
#include "WireCellUtil/NamedFactory.h"

#include "WireCellUtil/Testing.h"
#include "WireCellUtil/Units.h"

// cheating
#include "WireCellGen/TrackDepos.h"
#include "WireCellGen/Drifter.h"

#include <tbb/flow_graph.h>

namespace dfp = tbb::flow;
using namespace WireCell;
using namespace std;

// fixme: adapters should hold on to WC nodes via unique_ptr?

template<typename OutputType>
class TbbSourceAdapter {
public:
    // node type
    typedef ISendingNode<OutputType> sender_type;
    typedef std::shared_ptr< sender_type > sender_pointer;

    // data type
    typedef std::shared_ptr<const OutputType> output_pointer;

    TbbSourceAdapter( const TbbSourceAdapter& other ) : m_node(other.m_node) {}
    TbbSourceAdapter( const sender_pointer& node ) : m_node(node) {}
    TbbSourceAdapter( const INode::pointer& inode ) : m_node(dynamic_pointer_cast<sender_type>(inode)) {}

    ~TbbSourceAdapter() { }
    void operator=( const TbbSourceAdapter& other ) { m_node = other.m_node; }
    
    bool operator()(output_pointer& out) {
	Assert(m_node);
	return m_node->extract(out);
    }

private:
    sender_pointer m_node;
};    

template<typename InputType, typename OutputType>
class TbbConverterAdapter {
public:
    typedef IConverterNode<InputType,OutputType> converter_type;
    typedef std::shared_ptr< converter_type > converter_pointer;

    typedef typename std::shared_ptr<const InputType> input_pointer;
    typedef typename std::shared_ptr<const OutputType> output_pointer;

    // connect to TBB types
    typedef dfp::multifunction_node<std::shared_ptr<const InputType>,
				    dfp::tuple<std::shared_ptr<const OutputType> > > multi_node;
    typedef typename multi_node::output_ports_type output_ports_type;

    TbbConverterAdapter(const TbbConverterAdapter& other) : m_node(other.m_node) { }
    TbbConverterAdapter(const converter_pointer& node) : m_node(node) { }
    TbbConverterAdapter(const INode::pointer& inode) : m_node(dynamic_pointer_cast<converter_type>(inode)) { }
    ~TbbConverterAdapter() { }
    void operator=( const TbbConverterAdapter& other ) { m_node = other.m_node; }

    void operator()(const input_pointer& in, output_ports_type& out) {
	bool ok = m_node->insert(in);
	if (!ok) {
	    cerr << "Failed to insert\n";
	    // what do?
	}

	while (true) {
	    output_pointer o;
	    ok = m_node->extract(o);
	    if (!ok) {
		cerr << "Failed to extract\n";
		break;
	    }
	    std::get<0>(out).try_put(o); // errors?
	}
	return;
    }

private:
    converter_pointer m_node;

};


INode::pointer make_depo() {
    /// Here I cheat by directly making a TrackDepos.  Eventually,
    /// this would be replaced by using NamedFactor to get a
    /// TrackDepos object by class name and all the add_track() would
    /// be performed by configuration.

    TrackDepos* td = new TrackDepos;

    const double cm = units::cm;
    Ray same_point(Point(cm,-cm,cm), Point(10*cm,+cm,30*cm));

    const double usec = units::microsecond;

    td->add_track(10*usec, Ray(Point(cm,0,0), Point(2*cm,0,0)));
    td->add_track(20*usec, Ray(Point(cm,0,0), Point(cm,0,cm)));
    td->add_track(30*usec, Ray(Point(cm,-cm,-cm), Point(cm,cm,cm)));
    td->add_track(10*usec, same_point);
    td->add_track(100*usec, same_point);
    //td->add_track(1000*usec, same_point);
    //td->add_track(10000*usec, same_point);

    cerr << "Generated " << td->depos().size() << " depositions\n";

    return INode::pointer(td);
}

INode::pointer make_drifter(double to_x)
{
    return INode::pointer(new Drifter(to_x));
}

struct DepoChirp {
    std::string msg;

    typedef std::pair<int,int> counter_type;
    std::shared_ptr< counter_type > counter; // shared to survive copy 

    DepoChirp(const std::string& msg) : msg(msg), counter(new counter_type) {}
    DepoChirp(const DepoChirp& other) : msg(other.msg), counter(other.counter) {}
    void operator=( const DepoChirp& other ) { msg = other.msg; counter = other.counter; }

    IDepo::pointer operator()(const IDepo::pointer& depo) {
	++counter->first;
	cerr << msg;
	if (depo) {
	    cerr << ": depo: @" << depo->time() << " " << depo->pos() << endl;
	}
	else {
	    cerr << ": depo: NULL\n";
	    ++counter->second;
	}
	return depo;
    }
};


int main () {
    PluginManager& pm = PluginManager::instance();
    pm.add("WireCellGen");

    auto wp_wps = WireCell::Factory::lookup<IWireParameters>("WireParams");
    auto pw_gen = WireCell::Factory::lookup<IWireGenerator>("WireGenerator");
    Assert(pw_gen->insert(wp_wps));
    IWireGenerator::output_pointer wires;
    Assert(pw_gen->extract(wires));
    
    auto bc = WireCell::Factory::lookup<ICellMaker>("BoundCells");
    Assert(bc);
    Assert(bc->insert(wires));
    ICellMaker::output_pointer cells;
    Assert(bc->extract(cells));

    INode::pointer td = make_depo();
    Assert(td);
    cerr << "Got TrackDepo as INode" << endl;
    // auto p_ids = dynamic_pointer_cast< IDepoSource > (td);
    // Assert(p_ids);
    // cerr << "Got TrackDepo as IDepoSource" << endl;
    // auto p_isn = dynamic_pointer_cast< ISendingNode<IDepo> >(td);
    // Assert(p_isn);
    // cerr << "Got TrackDepo as ISendingNode<IDepo>" << endl;
    TbbSourceAdapter<IDepo> td_adapt(td);
    
    typedef TbbConverterAdapter<IDepo, IDepo> DepoDepoAdapt;

    INode::pointer du = make_drifter(wp_wps->pitchU().first.x());
    DepoDepoAdapt du_adapt(du);
    INode::pointer dv = make_drifter(wp_wps->pitchV().first.x());
    DepoDepoAdapt dv_adapt(dv);
    INode::pointer dw = make_drifter(wp_wps->pitchW().first.x());
    DepoDepoAdapt dw_adapt(dw);

    dfp::graph graph;

    dfp::source_node<IDepo::pointer> td_dfp(graph, td_adapt);
    
    DepoDepoAdapt::multi_node du_dfp(graph,1,du_adapt);
    DepoDepoAdapt::multi_node dv_dfp(graph,1,dv_adapt);
    DepoDepoAdapt::multi_node dw_dfp(graph,1,dw_adapt);

    typedef dfp::function_node<IDepo::pointer, IDepo::pointer> depo_func;
    
    DepoChirp depo_chirp_d("D");
    depo_func chirp_d(graph, 1, depo_chirp_d);
    DepoChirp depo_chirp_u("U");
    depo_func chirp_u(graph, 1, depo_chirp_u);
    DepoChirp depo_chirp_v("V");
    depo_func chirp_v(graph, 1, depo_chirp_v);
    DepoChirp depo_chirp_w("W");
    depo_func chirp_w(graph, 1, depo_chirp_w);

    dfp::broadcast_node<IDepo::pointer> fanout_dfp(graph);

    make_edge(td_dfp, chirp_d);
    make_edge(chirp_d, fanout_dfp);
    make_edge(fanout_dfp, du_dfp);
    make_edge(fanout_dfp, dv_dfp);
    make_edge(fanout_dfp, dw_dfp);
    make_edge(dfp::output_port<0>(du_dfp), chirp_u);
    make_edge(dfp::output_port<0>(dv_dfp), chirp_v);
    make_edge(dfp::output_port<0>(dw_dfp), chirp_w);

    td_dfp.activate();
    graph.wait_for_all();

    cerr << "Counts:\n"
	 << "\t#depo =" << depo_chirp_d.counter->first << "(null=" << depo_chirp_d.counter->second << ")\n"
	 << "\t#depoU=" << depo_chirp_u.counter->first << "(null=" << depo_chirp_u.counter->second << ")\n"
	 << "\t#depoV=" << depo_chirp_v.counter->first << "(null=" << depo_chirp_v.counter->second << ")\n"
	 << "\t#depoW=" << depo_chirp_w.counter->first << "(null=" << depo_chirp_w.counter->second << ")\n";
	

    return 0;
}
