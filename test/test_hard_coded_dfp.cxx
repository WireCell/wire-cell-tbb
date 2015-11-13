#include "WireCellIface/IWireParameters.h"
#include "WireCellIface/IWireGenerator.h"
#include "WireCellIface/SimpleNodes.h"

#include "WireCellIface/ICell.h"
#include "WireCellIface/ICellMaker.h"


#include "WireCellUtil/PluginManager.h"
#include "WireCellUtil/NamedFactory.h"

#include "WireCellUtil/Testing.h"
#include "WireCellUtil/Units.h"

// CHEATING.  Here is why the test is called "hard coded".  We do not
// want to have to know about these types but instead be driven by
// user configuration and work with base classes to build up the DFP
// graph.
#include "WireCellIface/IWireSelectors.h"
#include "WireCellGen/TrackDepos.h"
#include "WireCellGen/Drifter.h"
#include "WireCellGen/Diffuser.h"
#include "WireCellGen/PlaneDuctor.h"
#include "WireCellGen/Digitizer.h"
#include "WireCellAlg/ChannelCellSelector.h"

#include <sstream>
#include <tbb/flow_graph.h>

namespace dfp = tbb::flow;
using namespace WireCell;
using namespace std;

// fixme: adapters should hold on to WC nodes via unique_ptr?

template<typename OutputType>
class TbbSourceAdapter {
public:
    // for debugging. counts: (N_out, N_outerr)
    typedef std::vector<int> counter_type;

    // node type
    typedef ISendingNode<OutputType> sender_type;
    typedef std::shared_ptr< sender_type > sender_pointer;

    // data type
    typedef std::shared_ptr<const OutputType> output_pointer;

    // copy ctor
    TbbSourceAdapter( const TbbSourceAdapter& other ) : m_node(other.m_node), counter(other.counter) {}

    TbbSourceAdapter( const sender_pointer& node ) : m_node(node), counter(new counter_type(2)) {}
    TbbSourceAdapter( const INode::pointer& inode ) : m_node(dynamic_pointer_cast<sender_type>(inode)), counter(new counter_type(2)) {}

    ~TbbSourceAdapter() { }
    void operator=( const TbbSourceAdapter& other ) { m_node = other.m_node; }
    
    bool operator()(output_pointer& out) {
	Assert(m_node);
	bool ok = m_node->extract(out);
	if (ok) {
	    ++ counter->at(0);
	}
	else {
	    ++ counter->at(1);
	}
	return ok;
    }

    std::string chirp() {
	stringstream msg;
	msg << "counts:"
	    << " outok:" << counter->at(0)
	    << " outerr:" << counter->at(1)
	    << " --> " << typeid(OutputType).name() << "\n";
	return msg.str();
    }


private:
    sender_pointer m_node;
    std::shared_ptr< counter_type > counter; // shared to survive copy 
};    

template<typename InputType, typename OutputType>
class TbbConverterAdapter {
public:
    // for debugging. counts: (N_in, N_out, N_inerr, N_outerr)
    typedef std::vector<int> counter_type;

    typedef IConverterNode<InputType,OutputType> converter_type;
    typedef std::shared_ptr< converter_type > converter_pointer;

    typedef typename std::shared_ptr<const InputType> input_pointer;
    typedef typename std::shared_ptr<const OutputType> output_pointer;

    // connect to TBB types
    typedef dfp::multifunction_node<std::shared_ptr<const InputType>,
				    dfp::tuple<std::shared_ptr<const OutputType> > > multi_node;
    typedef typename multi_node::output_ports_type output_ports_type;

    // copy ctor
    TbbConverterAdapter(const TbbConverterAdapter& other) : m_node(other.m_node), counter(other.counter) {
	stringstream msg;
	msg << "TbbConverterAdapter copy " << (void*)this << " <-- " << (void*)&other
	    << " for " << typeid(InputType).name() << " --> " << typeid(OutputType).name() << "\n";
	cerr << msg.str();
    }
    TbbConverterAdapter(const converter_pointer& node) : m_node(node), counter(new counter_type(4)) {
    }
    TbbConverterAdapter(const INode::pointer& inode) : m_node(dynamic_pointer_cast<converter_type>(inode)), counter(new counter_type(4)) { }
    ~TbbConverterAdapter() { }
    void operator=( const TbbConverterAdapter& other ) {
	stringstream msg;
	msg << "TbbConverterAdapter assign " << (void*)this << " <-- " << (void*)&other
	    << " for " << typeid(InputType).name() << " --> " << typeid(OutputType).name() << "\n";
	cerr << msg.str();

	m_node = other.m_node;
    }

    void operator()(const input_pointer& in, output_ports_type& out) {
	bool ok = m_node->insert(in);
	if (ok) {
	    ++ counter->at(0);
	}
	else {
	    ++ counter->at(2);
	}

	while (true) {
	    output_pointer o;
	    ok = m_node->extract(o);
	    if (ok) {
		++ counter->at(1);
	    }
	    else {
		++ counter->at(3);
		break;
	    }
	    std::get<0>(out).try_put(o); // errors?
	}
	return;
    }

    std::string chirp() {
	stringstream msg;
	msg << "counts:"
	    << " inok:" << counter->at(0)
	    << " outok:" << counter->at(1)
	    << " inerr:" << counter->at(2)
	    << " outerr:" << counter->at(3)
	    << " " << typeid(InputType).name() << " --> " << typeid(OutputType).name() << "\n";
	return msg.str();
    }

    multi_node node(dfp::graph& graph, int concurrency=1) {
	return multi_node(graph, concurrency, *this);
    }



private:
    converter_pointer m_node;
    std::shared_ptr< counter_type > counter; // shared to survive copy 
};


// CHEATING.
INode::pointer make_depo() {
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

    cerr << "Generated " << td->depos().size() << " depositions (not counting EOS)\n";

    return INode::pointer(td);
}

// CHEATING.
INode::pointer make_drifter(double to_x)
{
    return INode::pointer(new Drifter(to_x));
}

// CHEATING.
INode::pointer make_diffusion(const Ray& pitch, double tick, double start_time, double time_offset)
{
    return INode::pointer(new Diffuser(pitch, tick, time_offset, start_time));
}

// CHEATING
INode::pointer make_ductor(const Ray& pitch,
				  WirePlaneLayer_t layer,
				  const IWire::shared_vector& wires,
				  double tick, double t0=0.0)
{
    WirePlaneId wpid(layer);

    const double pitch_distance = ray_length(pitch);
    const Vector pitch_unit = ray_unit(pitch);

    // get this planes wires sorted by index
    IWire::vector plane_wires;
    std::copy_if(wires->begin(), wires->end(),
		 back_inserter(plane_wires), select_uvw_wires[wpid.index()]);
    std::sort(plane_wires.begin(), plane_wires.end(), ascending_index);

    // number of wires and location of wire zero measured in pitch coordinate.
    const int nwires = plane_wires.size();
    IWire::pointer wire_zero = plane_wires[0];
    const Point to_wire = wire_zero->center() - pitch.first;
    const double wire_zero_dist = pitch_unit.dot(to_wire);

    // cerr << "Wire0 for plane=" << wpid.ident() << " distance=" << pitch_distance <<  " index=" << wire_zero->index() << endl;
    // cerr << "\twire ray=" << wire_zero->ray() << endl;
    // cerr << "\twire0 center=" << wire_zero->center() << endl;
    // cerr << "\tto wire=" << to_wire << endl;
    // cerr << "\tpitch = " << pitch_unit << endl;
    // cerr << "\twire0 dist=" << wire_zero_dist << endl;

    return INode::pointer(new PlaneDuctor(wpid, nwires, tick, pitch_distance, t0, wire_zero_dist));
}

INode::pointer make_digitizer(const IWire::shared_vector& wires)
{
    Digitizer* digi = new Digitizer;
    digi->set_wires(wires);
    return INode::pointer(digi);
}

INode::pointer make_ccselector(const ICell::shared_vector& cells)
{
    ChannelCellSelector* ccsel = new ChannelCellSelector;
    ccsel->set_cells(cells);
    return INode::pointer(ccsel);
}

template<typename DataType>
struct RepackUVW {
    typedef typename DataType::pointer pointer;
    typedef tbb::flow::tuple<pointer,pointer,pointer> tuple_type;
    typedef typename DataType::vector vector_type;
    typedef typename DataType::shared_vector return_type;

    return_type operator()( const tuple_type& tup ) {
	stringstream msg;
	pointer u = std::get<0>(tup);
	pointer v = std::get<1>(tup);
	pointer w = std::get<2>(tup);

	if (!u && !v && !w) {
	    cerr << "EOS from RepackUVW";
	    return return_type();
	}
	if (!u || !v || !w) {
	    cerr << "Error in RepackUVW, out of sync\n";
	    return return_type();
	}
	vector_type* ret = new vector_type;
	ret->push_back(u);
	ret->push_back(v);
	ret->push_back(w);
	return return_type(ret);
    }
};




int main () {
    PluginManager& pm = PluginManager::instance();
    pm.add("WireCellGen");

    // should be taken from global configuration
    const double tick = 2.0*units::microsecond; 
    const double start_time = 0.0*units::microsecond;
    const double drift_velocity = 1.6*units::millimeter/units::microsecond;

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

    // CHEATING.  We want to erase any explicit use of a nodes's input/output types.
    typedef TbbConverterAdapter<IDepo, IDepo> DepoDepoAdapt;
    typedef TbbConverterAdapter<IDepo, IDiffusion> DepoDiffusionAdapt;
    typedef TbbConverterAdapter<IDiffusion, IPlaneSlice> DiffusionPlaneSliceAdapt;
    typedef TbbConverterAdapter<IPlaneSlice::vector, IChannelSlice> PlaneSliceChannelSliceAdapt;
    typedef TbbConverterAdapter<IChannelSlice, ICellSlice> ChannelSliceCellSliceAdapt;

    // the data flow graph
    dfp::graph graph;

    // deposit
    INode::pointer td = make_depo();
    Assert(td);
    TbbSourceAdapter<IDepo> depo_adapt(td);
    dfp::source_node<IDepo::pointer> depo_source(graph, depo_adapt);
    
    // distribute deposition directly
    dfp::broadcast_node<IDepo::pointer> fanout_depo(graph);

    // drift depositions duplicitous 
    auto drift_u_adapt = DepoDepoAdapt(make_drifter(wp_wps->pitchU().first.x()));
    auto drift_v_adapt = DepoDepoAdapt(make_drifter(wp_wps->pitchV().first.x()));
    auto drift_w_adapt = DepoDepoAdapt(make_drifter(wp_wps->pitchW().first.x()));
    auto drift_u_node = drift_u_adapt.node(graph);
    auto drift_v_node = drift_v_adapt.node(graph);
    auto drift_w_node = drift_w_adapt.node(graph);


    // diffuse drifted depositions delicately
    auto diff_u_adapt = DepoDiffusionAdapt(make_diffusion(wp_wps->pitchU(), tick, start_time,
							  wp_wps->pitchU().first.x()/drift_velocity));
    auto diff_v_adapt = DepoDiffusionAdapt(make_diffusion(wp_wps->pitchV(), tick, start_time,
							  wp_wps->pitchV().first.x()/drift_velocity));
    auto diff_w_adapt = DepoDiffusionAdapt(make_diffusion(wp_wps->pitchW(), tick, start_time,
							  wp_wps->pitchW().first.x()/drift_velocity));
    auto diff_u_node = diff_u_adapt.node(graph);
    auto diff_v_node = diff_v_adapt.node(graph);
    auto diff_w_node = diff_w_adapt.node(graph);


    // ductor (con/in) digitizes diffusions deftly.
    auto duct_u_adapt = DiffusionPlaneSliceAdapt(make_ductor(wp_wps->pitchU(), kUlayer, wires, tick, start_time));
    auto duct_v_adapt = DiffusionPlaneSliceAdapt(make_ductor(wp_wps->pitchV(), kVlayer, wires, tick, start_time));
    auto duct_w_adapt = DiffusionPlaneSliceAdapt(make_ductor(wp_wps->pitchW(), kWlayer, wires, tick, start_time));
    auto duct_u_node = duct_u_adapt.node(graph);
    auto duct_v_node = duct_v_adapt.node(graph);
    auto duct_w_node = duct_w_adapt.node(graph);

    // detuplize
    typedef tbb::flow::tuple<IPlaneSlice::pointer,IPlaneSlice::pointer,IPlaneSlice::pointer> PlaneSliceTuple;
    dfp::join_node< PlaneSliceTuple > join_planeslice_node(graph);
    dfp::function_node< PlaneSliceTuple, IPlaneSlice::shared_vector > pst_repack(graph, 1, RepackUVW<IPlaneSlice>());

    // digitize dem diffused drifted depositions
    auto digi_adapt = PlaneSliceChannelSliceAdapt(make_digitizer(wires));
    auto digi_node = digi_adapt.node(graph);

    auto ccsel_adapt = ChannelSliceCellSliceAdapt(make_ccselector(cells));
    auto ccsel_node = ccsel_adapt.node(graph,4);

    // now knot nodes:
    make_edge(depo_source, fanout_depo);

    make_edge(fanout_depo, drift_u_node);
    make_edge(fanout_depo, drift_v_node);
    make_edge(fanout_depo, drift_w_node);

    make_edge(dfp::output_port<0>(drift_u_node), diff_u_node);
    make_edge(dfp::output_port<0>(drift_v_node), diff_v_node);
    make_edge(dfp::output_port<0>(drift_w_node), diff_w_node);

    make_edge(dfp::output_port<0>(diff_u_node), duct_u_node);
    make_edge(dfp::output_port<0>(diff_v_node), duct_v_node);
    make_edge(dfp::output_port<0>(diff_w_node), duct_w_node);

    make_edge(dfp::output_port<0>(duct_u_node), dfp::input_port<0>(join_planeslice_node));
    make_edge(dfp::output_port<0>(duct_v_node), dfp::input_port<1>(join_planeslice_node));
    make_edge(dfp::output_port<0>(duct_w_node), dfp::input_port<2>(join_planeslice_node));

    make_edge(join_planeslice_node, pst_repack);
    make_edge(pst_repack, digi_node);

    make_edge(dfp::output_port<0>(digi_node), ccsel_node);

    // flow, Morpheus, flow
    depo_source.activate();
    graph.wait_for_all();


    cerr << "Summary:\n"
	 << "Depositions: " << depo_adapt.chirp()
	 << "Drifted:\n"
	 << "\tU:" << drift_u_adapt.chirp()
	 << "\tV:" << drift_v_adapt.chirp()
	 << "\tW:" << drift_w_adapt.chirp()
	 << "Diffused:\n"
	 << "\tU:" << diff_u_adapt.chirp()
	 << "\tV:" << diff_v_adapt.chirp()
	 << "\tW:" << diff_w_adapt.chirp()
	 << "Ducted:\n"
	 << "\tU:" << duct_u_adapt.chirp()
	 << "\tV:" << duct_v_adapt.chirp()
	 << "\tW:" << duct_w_adapt.chirp()
	 << "Digitized: " << digi_adapt.chirp()
	 << "Celled: " << ccsel_adapt.chirp()
	;

    return 0;
}
