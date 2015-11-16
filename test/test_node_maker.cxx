#include "WireCellTbb/NodeMaker.h"

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

#include <chrono>
#include <thread>
void msleep(int msec)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(msec));
}


struct depo_chirp {
    std::string prefix;
    int sleep_ms;
    int count;
    depo_chirp(const std::string& pf, int ms=0) : prefix(pf), sleep_ms(ms), count(0) {}
    depo_pointer operator()(const depo_pointer& depo) {
	msleep(sleep_ms);
	++count;
	stringstream msg;
	msg << count+100000 << " " << this << " " << prefix << " depo: ";
	if (depo) {
	    msg << depo->time() << " --> " << depo->pos() << "\n";
	}
	else {
	    msg << "EOS\n";
	}
	cerr << msg.str();
	return depo;
    }
};

int main()
{
    // emulate looking up WC node via NF.
    WireCell::INode::pointer td = make_depo();
    cerr << "TrackDepos signature: " << td->signature() << endl;
    auto intypes = td->input_types();
    auto outtypes = td->output_types();
    Assert(intypes.empty());
    Assert(outtypes.size() == 1);
    cerr << "\tTrackDepo makes things of type: " << outtypes[0] << endl;

    
    // emulate registering all types of node makers and then looking up the corresponding one.
    WireCellTbb::SourceNodeMaker<WireCell::IDepoSource> depo_source_maker;
    cerr << "IDepoSource node maker signature: " << depo_source_maker.signature() << endl;

    Assert(td->signature() == depo_source_maker.signature());

    tbb::flow::graph graph;

    tbb::flow::graph_node* node = depo_source_maker.make_node(graph, td);


    // for expediency and testing we make some bare tbb nodes:
    tbb::flow::function_node<depo_pointer, depo_pointer> depo_chirp_node_sync(graph, 1, depo_chirp("sync "));
    tbb::flow::function_node<depo_pointer, depo_pointer> depo_chirp_node_async(graph, tbb::flow::unlimited, depo_chirp("async", 20));

    // emulate abstract connect method
    tbb::flow::source_node<depo_pointer>* depo_source_node = dynamic_cast<tbb::flow::source_node<depo_pointer>*>(node);
    Assert(depo_source_node);
    make_edge(*depo_source_node, depo_chirp_node_sync);
    make_edge(*depo_source_node, depo_chirp_node_async);
    depo_source_node->activate();
    graph.wait_for_all();

    return 0;
}
