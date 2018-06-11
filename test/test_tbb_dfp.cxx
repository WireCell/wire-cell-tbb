#include "WireCellTbb/DataFlowGraph.h"

#include "WireCellIface/IDepoSource.h"
#include "WireCellIface/IFrameFilter.h"
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
#include "WireCellGen/PlaneDuctor.h"
#include "WireCellGen/PlaneSliceMerger.h"
#include "WireCellGen/Digitizer.h"
#include "WireCellAlg/ChannelCellSelector.h"
#include "WireCellAlg/CellSliceSink.h"

// just cheat on this for debugging
#include "WireCellGen/WireParams.h"


#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

using namespace std;
using namespace WireCell;
using namespace WireCell::Gen;

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
INode::pointer make_drifter(WireCell::WirePlaneLayer_t layer)
{
    // WireParams wp;
    // double to_x = wp.pitch(layer).first.x();
    // return INode::pointer(new Drifter(to_x));
    return INode::pointer(new Drifter); // may no longer work
}


// cheats:
// diffuser needs wire pitch ray and time constants.
// need algorithmic configuration....
INode::pointer make_diffuser(WireCell::WirePlaneLayer_t layer)
{
    WireParams wp;
    auto pitch = wp.pitch(layer);
    cerr << "Diffuser: pitch:" << pitch
	 << " time_slice:" << time_slice
	 << " start_time:" << start_time
	 << " drift_velocity:" << drift_velocity
	 << endl;
    return INode::pointer(new Diffuser(pitch, time_slice, start_time, pitch.first.x()/drift_velocity));

}


// cheats:
// ductor needs a lot of derived wire info.
void config_ductor(WirePlaneId wpid)
{
    std::string uvw = "UVW";
    std::string name = "PlaneDuctor";

    cerr << "Getting plane ductor for " << wpid << endl;
    auto ductor = WireCell::Factory::lookup<IConfigurable>(name, name+uvw[wpid.index()]);

    // only slightly nonbogus config, don't expect this to be meaningful
    auto cfg = ductor->default_configuration();
    cfg["wpid"][0] = wpid.ilayer();
    cfg["wpid"][1] = 0;
    cfg["wpid"][2] = 0;
    cfg["nwires"] = 100;
    ductor->configure(cfg);

    // auto pitch = wp->pitch(layer);

    // const double pitch_distance = ray_length(pitch);
    // const Vector pitch_unit = ray_unit(pitch);
    // WirePlaneId wpid(layer);

    // // get this plane's wires sorted by index
    // IWire::vector plane_wires;
    // std::copy_if(wires->begin(), wires->end(),
    // 		 back_inserter(plane_wires), select_uvw_wires[wpid.index()]);
    // std::sort(plane_wires.begin(), plane_wires.end(), ascending_index);

    // // number of wires and location of wire zero measured in pitch coordinate.
    // const int nwires = plane_wires.size();
    // IWire::pointer wire_zero = plane_wires[0];
    // const Point to_wire = wire_zero->center() - pitch.first;
    // const double wire_zero_dist = pitch_unit.dot(to_wire);

    // return INode::pointer(new PlaneDuctor(wpid, nwires, time_slice, pitch_distance, start_time, wire_zero_dist));
}

INode::pointer make_psmerger()
{
    return INode::pointer(new PlaneSliceMerger());
}

INode::pointer make_digitizer()
{
    Digitizer* digi = new Digitizer;
    return INode::pointer(digi);
}

INode::pointer make_ccselector(const ICell::shared_vector& cells)
{
    ChannelCellSelector* ccsel = new ChannelCellSelector;
    return INode::pointer(ccsel);
}

void dump_to_bee_json(const std::string& filename, const ICellSliceSet& slices)
{
    const double drift_velocity = 1.6 * units::mm / units::microsecond;
    ofstream fstr(filename.c_str());
    fstr << "{\n";

    fstr << "\"x\":[";
    string comma = "";
    for (auto slice : slices) {
	if (!slice) { continue; }
	auto cells = slice->cells();
	if (!cells) { continue; }
	for (auto cell : *cells) {
	    double x = (slice->time() * drift_velocity) / units::cm;
	    fstr << comma << x;
	    comma = ", ";
	}
    }
    fstr << "],\n";

    string xyz="xyz";
    for (int ind=1; ind<3; ++ind) {
	fstr << "\"" << xyz[ind] << "\":[";
	string comma = "";
	for (auto slice : slices) {
	    if (!slice) { continue; }
	    auto cells = slice->cells();
	    if (!cells) { continue; }
	    for (auto cell : *cells) {
		double yz = cell->center()[ind] / units::cm;
		fstr << comma << yz;
		comma = ", ";
	    }
	}
	fstr << "],\n";
    }
    fstr << "\"type\":\"test_tbb_dfp\"\n";
    fstr << "}\n";
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

    config_ductor(WirePlaneId(kUlayer));
    config_ductor(WirePlaneId(kVlayer));
    config_ductor(WirePlaneId(kWlayer));



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

    WireCell::INode::pointer ductorU = WireCell::Factory::lookup<IPlaneDuctor>("PlaneDuctor", "PlaneDuctorU");
    WireCell::INode::pointer ductorV = WireCell::Factory::lookup<IPlaneDuctor>("PlaneDuctor", "PlaneDuctorV");
    WireCell::INode::pointer ductorW = WireCell::Factory::lookup<IPlaneDuctor>("PlaneDuctor", "PlaneDuctorW");

    WireCell::INode::pointer psmerger = WireCell::Factory::lookup<IPlaneSliceMerger>("PlaneSliceMerger");
    WireCell::INode::pointer digitizer = WireCell::Factory::lookup<IFrameFilter>("Digitizer");
    WireCell::INode::pointer ccselector = WireCell::Factory::lookup<IChannelCellSelector>("ChannelCellSelector");

    // special as we wanna use the real type 'cause we are lazy.
    auto cssptr = new CellSliceSink;
    WireCell::INode::pointer cssink(cssptr);

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
    Assert( dfp->connect(cell_maker, ccselector, 0, 1) );
    Assert( dfp->connect(ccselector, cssink) );

    //.... to be continued ...

    dfp->run();


    dump_to_bee_json("test_tbb_dfp.json", cssptr->slices());

    return 0;
}

