#include "WireCellTbb/TbbFlow.h"

#include "WireCellUtil/NamedFactory.h"
#include "WireCellUtil/String.h"
#include "WireCellUtil/DfpGraph.h"

#include <string>

WIRECELL_FACTORY(TbbFlow, WireCellTbb::TbbFlow, WireCell::IApplication, WireCell::IConfigurable);

using namespace WireCell;
using namespace WireCellTbb;

TbbFlow::TbbFlow()
{
}

TbbFlow::~TbbFlow()
{
}

Configuration TbbFlow::default_configuration() const
{
    std::string json = R"(
{
"dfp": "TbbDataFlowGraph",
"graph":[]
}
)";
    return configuration_loads(json, "json");
}

void TbbFlow::configure(const Configuration& cfg)
{
    std::string type, name, desc = get<std::string>("dfp","TbbDataFlowGraph");
    std::tie(type,name) = parse_pair(desc);
    m_dfp = Factory::lookup<IDataFlowGraph>(type, name);

    DfpGraph graph;
    graph.configure(cfg["graph"]);
    
    for (auto thc : graph.connections()) {
	auto tail_tn = get<0>(thc);
	auto head_tn = get<1>(thc);
	auto conn = get<2>(thc);

	INode::pointer tail_node = WireCell::Factory::lookup<INode>(tail_tn.type, tail_tn.name);
	INode::pointer head_node = WireCell::Factory::lookup<INode>(head_tn.type, head_tn.name);

	m_dfp->connect(tail_node, head_node, conn.tail, conn.head);
    }
}

void TbbFlow::execute()
{
    m_dfp->run();
}
