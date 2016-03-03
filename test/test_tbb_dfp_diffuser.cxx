#include "WireCellTbb/DataFlowGraph.h"

#include "WireCellIface/IDepoSource.h"
#include "WireCellIface/IWireParameters.h"
#include "WireCellIface/IWireSource.h"
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

// just cheat on this for debugging
#include "WireCellGen/WireParams.h"


#include <iostream>
#include <sstream>
#include <fstream>
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
    // td->add_track(10*usec, same_point);
    // td->add_track(100*usec, same_point);

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
INode::pointer make_drifter(WireCell::WirePlaneLayer_t layer)
{
    WireParams wp;
    double to_x = wp.pitch(layer).first.x();
    return INode::pointer(new Drifter(to_x));
}


// cheats:
// diffuser needs wire pitch ray and time constants.
// need algorithmic configuration....
INode::pointer make_diffuser(WireCell::WirePlaneLayer_t layer)
{
    WireParams wp;
    auto pitch = wp.pitch(layer);
    return INode::pointer(new Diffuser(pitch, time_slice, start_time, pitch.first.x()/drift_velocity));

}


int main(int argc, char* argv[])
{
    int max_threads = 0;
    if (argc>2) {
	max_threads = atoi(argv[1]);
    }

    PluginManager& pm = PluginManager::instance();
    pm.add("WireCellGen");
    pm.add("WireCellTbb");
    pm.add("WireCellAlg");

    // fixme: still faking NF lookup for DFP object
    WireCell::IDataFlowGraph* dfp = new WireCellTbb::DataFlowGraph(max_threads);

    WireCell::INode::pointer wire_source = WireCell::Factory::lookup<IWireSource>("WireSource");
    WireCell::INode::pointer cell_maker = WireCell::Factory::lookup<ICellMaker>("BoundCells");

    // emulate NF lookup and initialization
    WireCell::INode::pointer depo_source = make_depo();
    //WireCell::INode::pointer depo_source = WireCell::Factory::lookup<IDepoSource>("TrackDepos");

    // WireCell::INode::pointer drifterU = WireCell::Factory::lookup<IDrifter>("Drifter","DrifterU");
    // WireCell::INode::pointer drifterV = WireCell::Factory::lookup<IDrifter>("Drifter","DrifterV");
    // WireCell::INode::pointer drifterW = WireCell::Factory::lookup<IDrifter>("Drifter","DrifterW");
    WireCell::INode::pointer drifterU = make_drifter(WireCell::kUlayer);
    WireCell::INode::pointer drifterV = make_drifter(WireCell::kVlayer);
    WireCell::INode::pointer drifterW = make_drifter(WireCell::kWlayer);


    // WireCell::INode::pointer diffuserU = WireCell::Factory::lookup<IDiffuser>("Diffuser","DiffuserU");
    // WireCell::INode::pointer diffuserV = WireCell::Factory::lookup<IDiffuser>("Diffuser","DiffuserV");
    // WireCell::INode::pointer diffuserW = WireCell::Factory::lookup<IDiffuser>("Diffuser","DiffuserW");

    WireCell::INode::pointer diffuserU = make_diffuser(WireCell::kUlayer);
    WireCell::INode::pointer diffuserV = make_diffuser(WireCell::kVlayer);
    WireCell::INode::pointer diffuserW = make_diffuser(WireCell::kWlayer);

    cerr << "Connecting data flow graph:\n";

    Assert( dfp->connect(depo_source, drifterU) );
    Assert( dfp->connect(depo_source, drifterV) );
    Assert( dfp->connect(depo_source, drifterW) );

    Assert( dfp->connect(drifterU, diffuserU) );
    Assert( dfp->connect(drifterV, diffuserV) );
    Assert( dfp->connect(drifterW, diffuserW) );


    dfp->run();

    return 0;
}

