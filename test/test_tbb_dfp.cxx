#include "WireCellTbb/DataFlowGraph.h"

#include "WireCellIface/IDepoSource.h"
#include "WireCellIface/IWireParameters.h"
#include "WireCellIface/IWireGenerator.h"
#include "WireCellIface/IWire.h"
#include "WireCellIface/ICell.h"

#include "WireCellUtil/PluginManager.h"
#include "WireCellUtil/NamedFactory.h"
#include "WireCellUtil/Testing.h"
#include "WireCellUtil/Units.h"


// needed to prepare the plane ductor.  Making this go away is a
// problem.  Need to be able to derive plane ductor parameters from
// wire geometry.
#include "WireCellIface/IWireSelectors.h"

// These are for CHEATING to not use NF and config
#include "WireCellGen/WireGenerator.h"
#include "WireCellGen/BoundCells.h"
#include "WireCellGen/TrackDepos.h"
#include "WireCellGen/Drifter.h"
#include "WireCellGen/Diffuser.h"
#include "WireCellGen/PlaneDuctor.h"
#include "WireCellGen/PlaneSliceMerger.h"
#include "WireCellGen/Digitizer.h"
#include "WireCellAlg/ChannelCellSelector.h"
#include "WireCellAlg/CellSliceSink.h"


#include <iostream>
#include <sstream>
#include <string>

using namespace std;
using namespace WireCell;

// Cheat: should be taken from global configuration
const double time_slice = 2.0*units::microsecond; 
const double start_time = 0.0*units::microsecond;
const double drift_velocity = 1.6*units::millimeter/units::microsecond;

WireCell::INode::pointer make_depo() {
    using namespace WireCell;

    // get the track depo "properly"
    auto ids = WireCell::Factory::lookup<IDepoSource>("TrackDepos");
    Assert(ids);
    // but now cheat by upcasting since we don't have configuration finished yet 
    shared_ptr<TrackDepos> td = dynamic_pointer_cast<TrackDepos>(ids);

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

    INode::pointer node = dynamic_pointer_cast<INode>(ids);
    Assert(node);

    return node;
}


// cheats:
// Eventually each node will be given a "geometry" object from which to get the wire parameters.
// Eventually a drifter will be configured with a layer.
// or, simply have the to_x be a configuration parameter.
INode::pointer make_drifter(IWireParameters::pointer wp, WireCell::WirePlaneLayer_t layer)
{
    double to_x = wp->pitch(layer).first.x();
    return INode::pointer(new Drifter(to_x));
}


// cheats:
// diffuser needs wire pitch ray and time constants.
// need algorithmic configuration....
INode::pointer make_diffuser(IWireParameters::pointer wp, WireCell::WirePlaneLayer_t layer)
{
    auto pitch = wp->pitch(layer);
    return INode::pointer(new Diffuser(pitch, time_slice, start_time, pitch.first.x()/drift_velocity));
}


// cheats:
// ductor needs a lot of derived wire info.
INode::pointer make_ductor(IWireParameters::pointer wp, const IWire::shared_vector& wires,
			   WireCell::WirePlaneLayer_t layer)
{

    auto pitch = wp->pitch(layer);

    const double pitch_distance = ray_length(pitch);
    const Vector pitch_unit = ray_unit(pitch);
    WirePlaneId wpid(layer);

    // get this plane's wires sorted by index
    IWire::vector plane_wires;
    std::copy_if(wires->begin(), wires->end(),
		 back_inserter(plane_wires), select_uvw_wires[wpid.index()]);
    std::sort(plane_wires.begin(), plane_wires.end(), ascending_index);

    // number of wires and location of wire zero measured in pitch coordinate.
    const int nwires = plane_wires.size();
    IWire::pointer wire_zero = plane_wires[0];
    const Point to_wire = wire_zero->center() - pitch.first;
    const double wire_zero_dist = pitch_unit.dot(to_wire);

    return INode::pointer(new PlaneDuctor(wpid, nwires, time_slice, pitch_distance, start_time, wire_zero_dist));
}

INode::pointer make_psmerger()
{
    return INode::pointer(new PlaneSliceMerger());
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

WireCell::INode::pointer make_cellslicesink()
{
    return INode::pointer(new CellSliceSink);
}

int main(int argc, char* argv[])
{
    PluginManager& pm = PluginManager::instance();
    pm.add("WireCellGen");

    // set up wires and cells
    auto wire_param = WireCell::Factory::lookup<IWireParameters>("WireParams");
    auto wire_gen = WireCell::Factory::lookup<IWireGenerator>("WireGenerator");
    IWireGenerator::output_pointer wires;
    bool ok = (*wire_gen)(wire_param, wires);
    Assert(ok);
    auto bc = WireCell::Factory::lookup<ICellMaker>("BoundCells");
    Assert(bc);
    ICellMaker::output_pointer cells;
    ok = (*bc)(wires, cells);
    Assert(ok);
    
    int max_threads = 0;
    if (argc>2) {
	max_threads = atoi(argv[1]);
    }

    // emulate NF lookup
    WireCell::IDataFlowGraph* dfp = new WireCellTbb::DataFlowGraph(max_threads);

    // emulate NF lookup and initialization
    WireCell::INode::pointer depo_source = make_depo();

    WireCell::INode::pointer drifterU = make_drifter(wire_param, WireCell::kUlayer);
    WireCell::INode::pointer drifterV = make_drifter(wire_param, WireCell::kVlayer);
    WireCell::INode::pointer drifterW = make_drifter(wire_param, WireCell::kWlayer);

    WireCell::INode::pointer diffuserU = make_diffuser(wire_param, WireCell::kUlayer);
    WireCell::INode::pointer diffuserV = make_diffuser(wire_param, WireCell::kVlayer);
    WireCell::INode::pointer diffuserW = make_diffuser(wire_param, WireCell::kWlayer);

    WireCell::INode::pointer ductorU = make_ductor(wire_param, wires, WireCell::kUlayer);
    WireCell::INode::pointer ductorV = make_ductor(wire_param, wires, WireCell::kVlayer);
    WireCell::INode::pointer ductorW = make_ductor(wire_param, wires, WireCell::kWlayer);

    WireCell::INode::pointer psmerger = make_psmerger();
    WireCell::INode::pointer digitizer = make_digitizer(wires); // fixme: how to pass these in
    WireCell::INode::pointer ccselector = make_ccselector(cells); // fixme: ibid
    WireCell::INode::pointer cssink = make_cellslicesink();


    cerr << "Connecting data flow graph:\n";

    Assert( dfp->connect(depo_source, drifterU) );
    Assert( dfp->connect(depo_source, drifterV) );
    Assert( dfp->connect(depo_source, drifterW) );

    Assert( dfp->connect(drifterU, diffuserU) );
    Assert( dfp->connect(drifterV, diffuserV) );
    Assert( dfp->connect(drifterW, diffuserW) );

    Assert( dfp->connect(diffuserU, ductorU) );
    Assert( dfp->connect(diffuserV, ductorV) );
    Assert( dfp->connect(diffuserW, ductorW) );
    
    Assert( dfp->connect(ductorU, psmerger, 0, 0) );
    Assert( dfp->connect(ductorV, psmerger, 0, 1) );
    Assert( dfp->connect(ductorW, psmerger, 0, 2) );

    Assert( dfp->connect(psmerger, digitizer) );
    Assert( dfp->connect(digitizer, ccselector) );
    Assert( dfp->connect(ccselector, cssink) );

    //.... to be continued ...

    dfp->run();

    cout << "DepoSource: " << depo_source->nin() << "/" << depo_source->nout()
	 << endl;

    cout << "Drifter"
	 << " U:" << drifterU->nin() << "/" << drifterU->nout()
	 << " V:" << drifterV->nin() << "/" << drifterV->nout()
	 << " W:" << drifterW->nin() << "/" << drifterW->nout()
	 << endl;

    cout << "Diffuser"
	 << " U:" << diffuserU->nin() << "/" << diffuserU->nout()
	 << " V:" << diffuserV->nin() << "/" << diffuserV->nout()
	 << " W:" << diffuserW->nin() << "/" << diffuserW->nout()
	 << endl;

    cout << "Ductor"
	 << " U:" << ductorU->nin() << "/" << ductorU->nout()
	 << " V:" << ductorV->nin() << "/" << ductorV->nout()
	 << " W:" << ductorW->nin() << "/" << ductorW->nout()
	 << endl;

    cout << "Merger:" << psmerger->nin() << "/" << psmerger->nout() << endl;
    cout << "Digitizer:" << digitizer->nin() << "/" << digitizer->nout() << endl;
    cout << "CellSelector:" << ccselector->nin() << "/" << ccselector->nout() << endl;
    
    return 0;
}
