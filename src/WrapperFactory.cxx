#include "WireCellTbb/WrapperFactory.h"
#include "WireCellTbb/SourceCat.h"
#include "WireCellTbb/SinkCat.h"
#include "WireCellTbb/JoinCat.h"
#include "WireCellTbb/QueuedoutCat.h"

using namespace WireCell;
using namespace WireCellTbb;

	
WrapperFactory::WrapperFactory(tbb::flow::graph& graph)
    : m_graph(graph)
{
    // gotta add one for each Wire Cell category
    bind_maker<SourceNodeWrapper>(INode::sourceNode);
    bind_maker<SinkNodeWrapper>(INode::sinkNode);
    bind_maker<QueuedoutWrapper>(INode::queuedoutNode);
    bind_maker<JoinWrapper>(INode::joinNode);
    // fixme: add join, function, ...
}

Node WrapperFactory::operator()(INode::pointer wcnode)
{
    auto nit = m_nodes.find(wcnode);
    if (nit != m_nodes.end()) {
	return nit->second;
    }

    auto mit = m_factory.find(wcnode->category());
    if (mit == m_factory.end()) {
	return nullptr;
    }
    auto maker = mit->second;

    Node node = (*maker)(m_graph, wcnode);
    m_nodes[wcnode] = node;
    return node;
}
