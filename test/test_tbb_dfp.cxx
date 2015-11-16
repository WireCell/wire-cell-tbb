#include "WireCellTbb/DataFlowGraph.h"

#include "WireCellGen/TrackDepos.h"
#include "WireCellUtil/Testing.h"

#include <iostream>
#include <sstream>
#include <string>
using namespace std;

WireCell::INode::pointer make_depo() {
    using namespace WireCell;
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

typedef WireCell::IDepo::pointer depo_pointer;

int main()
{
    // emulate NF lookup
    WireCell::IDataFlowGraph* dfp = new WireCellTbb::DataFlowGraph;

    WireCell::INode::pointer td = make_depo();

    bool ok = dfp->connect(td, td);
    Assert(!ok);

    //.... to be continued ...

    return 0;
}
