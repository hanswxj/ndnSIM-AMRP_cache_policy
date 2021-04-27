// ndn-simple-lirs.cpp

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

namespace ns3 {

/**
 * This scenario simulates a very simple network topology:
 *
 *
 *      +----------+     1Mbps      +--------+     1Mbps      +----------+
 *      | consumer | <------------> | router | <------------> | producer |
 *      +----------+         10ms   +--------+          10ms  +----------+
 *
 *
 * Consumer requests data from producer with frequency 10 interests per second
 * (interests contain constantly increasing sequence number).
 *
 * For every received interest, producer replies with a data packet, containing
 * 1024 bytes of virtual payload.
 *
 * To run scenario and see what is happening, use the following command:
 *
 *     NS_LOG=ndn.Consumer:ndn.Producer ./waf --run=ndn-simple
 */

int
main(int argc, char* argv[])
{
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::QueueBase::MaxPackets", UintegerValue(20));

  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Creating nodes
  NodeContainer nodes;
  nodes.Create(2);        //原来是3

  // Connecting nodes using two links
  PointToPointHelper p2p;
  p2p.Install(nodes.Get(0), nodes.Get(1));
  //p2p.Install(nodes.Get(1), nodes.Get(2));

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper,ndnHelper1;

  ndnHelper.SetOldContentStore("ns3::ndn::cs::Nocache");
  
  ndnHelper.SetDefaultRoutes(true);
  // ndnHelper.setCsSize(0);
  // ndnHelper.setPolicy("nfd::cs::lru");
  ndnHelper.Install(nodes.Get(0));

  ndnHelper1.SetDefaultRoutes(true);
  ndnHelper1.setCsSize(100);
  ndnHelper1.setPolicy("nfd::cs::lru");
  ndnHelper1.Install(nodes.Get(1));

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/multicast");
  //ndn::StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/best-route");

  // Installing applications

  // Consumer
  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerZipfMandelbrot");
  consumerHelper.SetPrefix("/prefix");
  consumerHelper.SetAttribute("Frequency", StringValue("100"));        // 10 interests a second
  consumerHelper.SetAttribute("NumberOfContents", StringValue("1000")); // 100 different contents
  consumerHelper.SetAttribute("q",StringValue("0.7"));
  consumerHelper.SetAttribute("s",StringValue("0.7"));

  //ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
     // Consumer will request /prefix/0, /prefix/1, ...
  //consumerHelper.SetPrefix("/prefix");
  //consumerHelper.SetAttribute("Frequency", StringValue("10")); // 10 interests a second
  consumerHelper.Install(nodes.Get(0));                        // first node

  // Producer
  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  // Producer will reply to all requests starting with /prefix
  producerHelper.SetPrefix("/prefix");
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(nodes.Get(1)); // last node,原来是2

  
  Simulator::Stop(Seconds(20.0));

  //ndn::CsTracer::InstallAll("cs-trace-lirs.txt",Seconds(1));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
