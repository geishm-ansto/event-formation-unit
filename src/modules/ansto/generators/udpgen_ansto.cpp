/** Copyright (C) 2016, 2017 European Spallation Source ERIC */

#include <CLI/CLI.hpp>
#include <ansto/common/ParserFactory.h>
#include <ansto/generators/igenbase.h>
#include <ansto/generators/mcpd8.h>
#include <ansto/generators/bnlpz.h>
#include <arpa/inet.h>
#include <cassert>
#include <cinttypes>
#include <common/Socket.h>
#include <common/ZmqInput.h>
#include <common/Timer.h>
#include <memory>
#include <string.h>
#include <string>
#include <unistd.h>
// GCOVR_EXCL_START

using namespace::Ansto;
using namespace Ansto::Generator;

struct {
  std::string Detector{"mcpd8"};
  std::string FileName{""};
  std::string ConfigFile{""};
  std::string IpAddress{"127.0.0.1"};
  uint16_t UDPPort{9000};
  uint64_t NumberOfPackets{0}; // 0 == all packets
  uint64_t SpeedThrottle{0};   // 0 is fastest higher is slower
  uint64_t PktThrottle{0};     // 0 is fastest
  bool Loop{false};            // Keep looping the same file forever
  // Not yet CLI settings
  uint32_t KernelTxBufferSize{1000000};
} Settings;

CLI::App app{"ANSTO simulator or file to UDP data generator"};

int main(int argc, char *argv[]) {
  app.add_option("-d, --detector", Settings.Detector, "Detector type");
  app.add_option("-f, --file", Settings.FileName, "Raw detector file as input");
  app.add_option("-i, --ip", Settings.IpAddress, "Destination IP address");
  app.add_option("-p, --port", Settings.UDPPort, "Destination UDP port");
  app.add_option("-a, --packets", Settings.NumberOfPackets,
                 "Number of packets to send");
  app.add_option("-t, --throttle", Settings.SpeedThrottle,
                 "Speed throttle (0 is fastest, larger is slower)");
  app.add_option("-s, --pkt_throttle", Settings.PktThrottle,
                 "Extra usleep() after n packets");
  app.add_option("-c, --config", Settings.ConfigFile,
                 "Detector specific configuration file");
  CLI11_PARSE(app, argc, argv);

  char buffer[10000];
  const int MaxBufferSize = 8900;

  Socket::Endpoint local("0.0.0.0", 0);
  Socket::Endpoint remote(Settings.IpAddress.c_str(), Settings.UDPPort);

  void *senderObject;
  std::unique_ptr<ZMQPublisher> zmqPtr;
  std::unique_ptr<UDPTransmitter> udpPtr;
  std::function<int(void *, void *, int)> Sender;

  // create an instance of the source
  std::unique_ptr<ISource> source;
  if (Settings.Detector == "mcpd8") {
    udpPtr = std::make_unique<UDPTransmitter>(local, remote);
    udpPtr->setBufferSizes(Settings.KernelTxBufferSize, 0);
    udpPtr->printBufferSizes();
    senderObject = static_cast<void*>(&(*udpPtr));
    Sender = InvokeSend<UDPTransmitter>;
    source =
        std::make_unique<Mcpd8Source>(Settings.FileName, Settings.ConfigFile);
  } else if (Settings.Detector == "bnlpz") {
    zmqPtr = std::make_unique<ZMQPublisher>(remote);
    senderObject = static_cast<void*>(&(*zmqPtr));
    Sender = InvokeSend<ZMQPublisher>;
    source =
        std::make_unique<BnlpzSource>(Settings.FileName, Settings.ConfigFile);
  }
  else {
    printf("Detector %s not found\n", Settings.Detector.c_str());
    return 1;
  }

  uint64_t packets = 0;
  uint64_t totpackets = 0;
  Timer timer;

  int rdsize;
  while ((rdsize = source->Next((char *)&buffer, MaxBufferSize)) != -1) {
    if (rdsize == 0) {
      printf("no data returned - ignoring\n");
      continue; //
    }

    Sender(senderObject, buffer, rdsize);
    if (Settings.SpeedThrottle) {
      usleep(Settings.SpeedThrottle);
    }
    packets++;
    totpackets++;
    if (Settings.PktThrottle) {
      if (totpackets % Settings.PktThrottle == 0) {
        usleep(10);
      }
    }
    if (Settings.NumberOfPackets != 0 and packets >= Settings.NumberOfPackets) {
      packets = 0;
      break;
    }
  }

  uint64_t secs = timer.timeus() / 1000000;
  printf("Sent %" PRIu64 " packets in %" PRIu64 " secs\n", totpackets, secs);

  return 0;
}
// GCOVR_EXCL_STOP
