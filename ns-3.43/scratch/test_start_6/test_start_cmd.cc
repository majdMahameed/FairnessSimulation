// Dumbbell Topology version preserving full Fat Tree logic and configuration

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/output-stream-wrapper.h"
#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <tuple>
#include <utility>
#include <sstream>
#include <string>
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TestRTTDifference6"); // define logging component so NS_LOG_* macros work

std::string tcpProtocol = "NewReno"; // default TCP protocol

// ───────────── CWND Tracing Helpers ─────────────
static std::vector<double> lastCwnd;
static std::vector<double> lastRtt;

static void
RttTracer(uint32_t flowId, Time oldRtt, Time newRtt)
{
    if (flowId < lastRtt.size()) {
        lastRtt[flowId] = newRtt.GetSeconds();
    }
}

static void
CwndTracer(Ptr<OutputStreamWrapper> stream,
    std::string protocol,
    uint32_t flowId,
    uint32_t oldCwnd,
    uint32_t newCwnd)
{
    static bool headerWritten = false;
    if (!headerWritten) {
        std::ostream* os = stream->GetStream();
        std::streampos pos = os->tellp();
        if (pos == 0) {
            *os << "Protocol,Time,Flow,Cwnd,RTT,Throughput_cwnd_rtt_bps\n";
        }
        headerWritten = true;
    }
    double rtt = (flowId < lastRtt.size()) ? lastRtt[flowId] : 0.0;
    double throughput = (rtt > 0) ? (double)newCwnd * 8 / rtt : 0.0; // bits/sec
    if (flowId < lastCwnd.size()) {
        lastCwnd[flowId] = newCwnd;
    }
    *(stream->GetStream()) << protocol << ','
                           << Simulator::Now().GetSeconds() << ','
                           << flowId << ','
                           << newCwnd << ','
                           << rtt << ','
                           << throughput << '\n';
}

static void
SetupCwndTracing(Ptr<BulkSendApplication> app,
                 Ptr<OutputStreamWrapper> stream,
                 std::string protocol,
                 uint32_t flowId)
{
    Ptr<Socket> sock = app->GetSocket();
    if (sock)
    {
        // Trace cwnd
        sock->TraceConnectWithoutContext(
            "CongestionWindow",
            MakeBoundCallback(&CwndTracer, stream, protocol, flowId));
        // Trace RTT
        sock->TraceConnectWithoutContext(
            "RTT",
            MakeBoundCallback(&RttTracer, flowId));
    }
}

int main(int argc, char *argv[]) {
    uint32_t numFlows = 5; // default value
    std::string dataSendRate = "0.5Mbps"; // default data rate
    std::string rttDelays = "5ms"; // default: all flows get 10ms
    std::string bottleneckLinkDataRate = "1.2Mbps"; // default bottleneck link data rate
    std::string bottleneckLinkDelay = "1ms"; // default bottleneck link delay
    std::string simulationTime = "100.0"; // default simulation time
    uint32_t fileSize = 0; // default: 0 means unlimited
    double startJitterMax = 0.2; // max random start offset for senders (seconds)

    CommandLine cmd;
    cmd.AddValue("numFlows", "Number of flows (number of left/right nodes)", numFlows);
    cmd.AddValue("DataSendRate","Data rate for sending packets", dataSendRate);
    cmd.AddValue("RttDelays", "Comma-separated list of RTT delays for each flow (e.g., 10us,50us,100us,1ms)", rttDelays);
    cmd.AddValue("BottleneckLinkDataRate", "Data rate for the bottleneck link", bottleneckLinkDataRate);
    cmd.AddValue("BottleneckLinkDelay", "Delay for the bottleneck link", bottleneckLinkDelay);
    cmd.AddValue("SimulationTime", "Total simulation time in seconds", simulationTime);
    cmd.AddValue("TcpProtocol", "TCP protocol: NewReno, BBR, DCTCP, Cubic", tcpProtocol);
    cmd.AddValue("FileSize", "File size to send per flow (bytes, 0 for unlimited)", fileSize);
    cmd.AddValue("StartJitterMax", "Maximum random start offset for sender apps in seconds (uniform [0,StartJitterMax])", startJitterMax);
    cmd.Parse(argc, argv);
    // RNG for small randomized start times
    std::random_device rd;
    std::mt19937 jitterGen(rd());
    std::uniform_real_distribution<double> jitterDist(0.0, startJitterMax);

    // Parse rttDelays into a vector for per-flow RTTs
    std::vector<std::string> rttDelayList;
    std::stringstream ss(rttDelays);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item.erase(std::remove_if(item.begin(), item.end(), ::isspace), item.end());
        rttDelayList.push_back(item);
    }
    // If fewer RTTs than flows, repeat the last value
    while (rttDelayList.size() < numFlows) {
        rttDelayList.push_back(rttDelayList.back());
    }

    // Set the TCP protocol and protocol-specific options
    std::string protocolTypeId;
    bool useRed = false;
    if (tcpProtocol == "NewReno") {
        protocolTypeId = "ns3::TcpNewReno";
        Config::SetDefault("ns3::TcpL4Protocol::RecoveryType", StringValue("ns3::TcpClassicRecovery"));
    } else if (tcpProtocol == "BBR") {
        protocolTypeId = "ns3::TcpBbr";
        // BBR uses its own recovery, no need to set RecoveryType
    } else if (tcpProtocol == "DCTCP") {
        protocolTypeId = "ns3::TcpDctcp";
        useRed = true; // Only DCTCP uses RED+ECN
    } else if (tcpProtocol == "Cubic") {
        protocolTypeId = "ns3::TcpCubic";
        Config::SetDefault("ns3::TcpL4Protocol::RecoveryType", StringValue("ns3::TcpClassicRecovery"));
    } else {
        NS_ABORT_MSG("Invalid TCP protocol: " << tcpProtocol);
    }
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(protocolTypeId));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(1));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(1)); // disables delayed ACKs
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(false));

    // Create node containers
    NodeContainer leftNodes, rightNodes, routers;
    leftNodes.Create(numFlows);
    rightNodes.Create(numFlows);
    routers.Create(2);

    // Store device containers for each link
    std::vector<NetDeviceContainer> leftDevices, rightDevices;
    NetDeviceContainer bottleneckDevices;

    // Set up point-to-point helper for access links (left side)
    for (uint32_t i = 0; i < numFlows; ++i) {
        PointToPointHelper accessLink;
        accessLink.SetDeviceAttribute("DataRate", StringValue(dataSendRate));
        accessLink.SetChannelAttribute("Delay", StringValue(rttDelayList[i]));
        leftDevices.push_back(accessLink.Install(leftNodes.Get(i), routers.Get(0)));
    }

    // Set up point-to-point helper for access links (right side)
    for (uint32_t i = 0; i < numFlows; ++i) {
        PointToPointHelper accessLink;
        accessLink.SetDeviceAttribute("DataRate", StringValue(dataSendRate));
        accessLink.SetChannelAttribute("Delay", StringValue("10ms")); // Fixed 10ms delay for all right links
        rightDevices.push_back(accessLink.Install(rightNodes.Get(i), routers.Get(1)));
    }

    // Set up point-to-point helper for the bottleneck link
    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckLinkDataRate));
    bottleneckLink.SetChannelAttribute("Delay", StringValue(bottleneckLinkDelay));
    if (!useRed) {
        bottleneckLink.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("180p")); // DropTail for all except DCTCP
    }
    bottleneckDevices = bottleneckLink.Install(routers.Get(0), routers.Get(1));

    TrafficControlHelper tch;

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(leftNodes);
    internet.Install(rightNodes);
    internet.Install(routers);

    if (useRed) {
        // Only DCTCP installs RED with ECN
        Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(true));
        Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(2000)); // packets
        Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(2000)); // packets
        Config::SetDefault("ns3::RedQueueDisc::MaxSize", QueueSizeValue(QueueSize("2000p")));
        tch.SetRootQueueDisc("ns3::RedQueueDisc", "UseEcn", BooleanValue(true));
        tch.Install(bottleneckDevices);
    }

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> leftInterfaces, rightInterfaces;

    // Assign addresses to left links
    address.SetBase("10.1.0.0", "255.255.255.0");
    for (uint32_t i = 0; i < numFlows; ++i) {
        leftInterfaces.push_back(address.Assign(leftDevices[i]));
        address.NewNetwork();
    }

    // Assign addresses to right links
    address.SetBase("10.2.0.0", "255.255.255.0");
    for (uint32_t i = 0; i < numFlows; ++i) {
        rightInterfaces.push_back(address.Assign(rightDevices[i]));
        address.NewNetwork();
    }

    // Assign address to bottleneck link
    address.SetBase("10.3.0.0", "255.255.255.0");
    address.Assign(bottleneckDevices);
    
    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Convert simulationTime string to double for use in Seconds()
    double simTime = std::stod(simulationTime);

    uint16_t port = 8080; // Port for applications
    ApplicationContainer sender, receiver;

    // ───────────── CWND Trace File Setup ─────────────
    std::string cwndFile = "scratch/test_start_6/results/cwnd_trace.csv";
    // Overwrite the file each run
    Ptr<OutputStreamWrapper> cwndStream = Create<OutputStreamWrapper>(cwndFile, std::ios_base::out);

    // Prepare vectors for last cwnd and RTT per flow
    lastCwnd.assign(numFlows + 1, 0.0); // flowId is 1-based
    lastRtt.assign(numFlows + 1, 0.0);

    std::vector<double> flowStartOffsets(numFlows, 0.0);
    flowStartOffsets[0] = jitterDist(jitterGen); // First flow: random jitter
    double increase = 5.0;

    for (uint32_t i = 1; i < numFlows; ++i) {
            flowStartOffsets[i] = flowStartOffsets[0] + increase;
            increase += 5.0; // Second flow: 15s after first (including jitter)
    }



    for (uint32_t i = 0; i < numFlows; ++i) {
        // Create a PacketSink on the right node
        Address sinkAddress(InetSocketAddress(rightInterfaces[i].GetAddress(0), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
        receiver = sinkHelper.Install(rightNodes.Get(i));
        receiver.Start(Seconds(0.0));
        receiver.Stop(Seconds(simTime));

        // Create a BulkSend application on the left node
        BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
        if (fileSize > 0) {
            bulkSender.SetAttribute("MaxBytes", UintegerValue(fileSize));
        } else {
            bulkSender.SetAttribute("MaxBytes", UintegerValue(0)); // 0 means unlimited
        }
        bulkSender.SetAttribute("SendSize", UintegerValue(1024)); // 1024 bytes per segment
        ApplicationContainer senderApp = bulkSender.Install(leftNodes.Get(i));

        // start with a small random offset so flows are slightly staggered (e.g., 0s and 0.2s)
            double startOffset = (i < flowStartOffsets.size()) ? flowStartOffsets[i] : jitterDist(jitterGen);
            senderApp.Start(Seconds(startOffset));
            senderApp.Stop(Seconds(simTime));

        Ptr<BulkSendApplication> bulkApp = DynamicCast<BulkSendApplication>(senderApp.Get(0));
        if (bulkApp)
        {
            // Schedule tracing shortly after the app's start time so the socket exists.
            // Previously this used a fixed 0.01s which could run before the app/socket
            // was started when startOffset > 0, causing tracing not to attach.
            double traceTime = startOffset + 0.01;
            Simulator::Schedule(Seconds(traceTime), &SetupCwndTracing, bulkApp, cwndStream, tcpProtocol, i+1);
        }
    }
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Set up Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Analyze FlowMonitor results
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    uint64_t totalTxPackets = 0, totalRxPackets = 0, totalTxBytes = 0, totalRxBytes = 0;
    double totalDelay = 0.0, totalRxThroughput = 0.0, totalTxThroughput = 0.0, totalLossRatio = 0.0;
    uint32_t flowCount = 0;

    // Prepare per-flow throughput vectors
    std::vector<double> perFlowRxThroughput(numFlows, 0.0);
    std::vector<double> perFlowTxThroughput(numFlows, 0.0);

    for (const auto& flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        // Only include flows from leftNodes to rightNodes (forward direction)
        for (uint32_t i = 0; i < numFlows; ++i) {
            if (t.sourceAddress == leftInterfaces[i].GetAddress(0) &&
                t.destinationAddress == rightInterfaces[i].GetAddress(0)) {
                double duration = flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds();
                double rxThroughput = 0.0, txThroughput = 0.0;
                if (duration > 0) {
                    rxThroughput = (flow.second.rxBytes * 8.0 / duration) / 1e3; // Kbps
                    txThroughput = (flow.second.txBytes * 8.0 / duration) / 1e3; // Kbps
                }
                perFlowRxThroughput[i] = rxThroughput;
                perFlowTxThroughput[i] = txThroughput;
                break;
            }
        }
    }

    std::cout << "\nPer-flow throughput (Kbps):\n";
    for (size_t i = 0; i < perFlowRxThroughput.size(); ++i) {
        std::cout << "  Flow " << (i+1) << " sender: " << perFlowTxThroughput[i]
                  << " Kbps, receiver: " << perFlowRxThroughput[i] << " Kbps" << std::endl;
    }
    // Print RTT delays for each flow
    for (uint32_t i = 0; i < numFlows; ++i) {
        std::cout << "Flow " << i << " RTT delay: " << rttDelayList[i] << std::endl;
    }
    // Jain's Fairness Index calculation (using receiver throughput)
    double sum = 0.0, sumSq = 0.0;
    for (double thr : perFlowRxThroughput) {
        sum += thr;
        sumSq += thr * thr;
    }
    double jainIndex = (numFlows > 0 && sumSq > 0) ? (sum * sum) / (numFlows * sumSq) : 0.0;

    std::cout << "\n===== Simulation Results =====\n";
    std::cout << "Protocol: " << tcpProtocol << "\n";
    for (size_t i = 0; i < perFlowRxThroughput.size(); ++i) {
        std::cout << "Flow " << (i+1)
                  << " | Throughput: " << perFlowRxThroughput[i] / 1000.0 << " Mbps"
                  << " | RTT: " << rttDelayList[i] << "\n";
    }
    std::cout << "Jain's Fairness Index (receiver throughput): " << jainIndex << "\n";
    std::cout << "=============================\n";

    // Write per-experiment results to a new CSV file (one row per experiment)
    std::string summaryFile = "scratch/test_start_6/results/results_summary.csv";
    std::vector<std::string> expectedHeaders;
    expectedHeaders.push_back("Protocol");
    for (size_t i = 0; i < perFlowRxThroughput.size(); ++i) {
        expectedHeaders.push_back("Flow" + std::to_string(i+1) + "_Mbps");
    }
    for (size_t i = 0; i < perFlowRxThroughput.size(); ++i) {
        expectedHeaders.push_back("Flow" + std::to_string(i+1) + "_RTT");
    }
    expectedHeaders.push_back("JainIndex");

    // Check if file exists and if header matches
    bool writeHeader = false;
    bool truncateFile = false;
    std::ifstream checkFile(summaryFile);
    if (checkFile.good()) {
        std::string firstLine;
        std::getline(checkFile, firstLine);
        std::stringstream ss(firstLine);
        std::string col;
        std::vector<std::string> fileHeaders;
        while (std::getline(ss, col, ',')) {
            fileHeaders.push_back(col);
        }
        if (fileHeaders != expectedHeaders) {
            truncateFile = true;
        }
    } else {
        writeHeader = true;
        truncateFile = true;
    }
    checkFile.close();

    std::ofstream csvFile;
    if (truncateFile) {
        csvFile.open(summaryFile, std::ios::out | std::ios::trunc);
        writeHeader = true;
    } else {
        csvFile.open(summaryFile, std::ios::app);
    }

    if (writeHeader) {
        for (size_t i = 0; i < expectedHeaders.size(); ++i) {
            csvFile << expectedHeaders[i];
            if (i + 1 < expectedHeaders.size()) csvFile << ",";
        }
        csvFile << "\n";
    }

    // Write data row
    csvFile << tcpProtocol;
    for (size_t i = 0; i < perFlowRxThroughput.size(); ++i) {
        csvFile << "," << perFlowRxThroughput[i] / 1000.0;
    }
    for (size_t i = 0; i < perFlowRxThroughput.size(); ++i) {
        csvFile << "," << rttDelayList[i];
    }
    csvFile << "," << jainIndex << std::endl;
    csvFile.close();

    Simulator::Destroy();
    return 0;
}
