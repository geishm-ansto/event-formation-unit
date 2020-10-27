#include "Timestamp.h"
#include "../adc_readout/UDPClient.h"
#include <chrono>
#include "../adc_readout/AdcTimeStamp.h"
#include <common/DetectorModuleRegister.h>

using std::chrono_literals::operator""ms;

Timestamp::Timestamp(const BaseSettings &Settings) : Detector("timestamp", Settings), Settings(Settings), Service(std::make_shared<asio::io_service>()),
                                                     Worker(*Service), KafkaProducer(Settings.KafkaBroker, Settings.KafkaTopic), Serializer("steven_timestamps", 1000, 200ms, &KafkaProducer, EventSerializer::TimestampMode::INDEPENDENT_EVENTS, OffsetTime(OffsetTime::Offset::NONE)) {
  std::function<void()> inputFunc = [this]() { this->threadFunc(); };
  Detector::AddThreadFunction(inputFunc, "input");
}


void Timestamp::threadFunc() {
  auto ProcessPacket = [&](auto Packet) {
    RawTimeStamp *TimePtr;
    if (Packet.Length >= 8) {
      TimePtr = reinterpret_cast<RawTimeStamp*>(&Packet.Data);
      TimePtr->fixEndian();
      auto CurrentTS = TimeStamp(*TimePtr, TimeStamp::ClockMode::External88Mhz);
      auto Evt = std::make_unique<EventData>();
      Evt->Timestamp = CurrentTS.getTimeStampNS();
      Serializer.addEvent(std::move(Evt));
    }
  };
  auto UdpReceiver = std::make_unique<UDPClient>(Service, EFUSettings.DetectorAddress, EFUSettings.DetectorPort, ProcessPacket);
  Service->run();
}

void Timestamp::stopThreads() {
  Service->stop();
  Detector::stopThreads();
}

void CLIArguments(CLI::App &) {
}

REGISTER_MODULE(Timestamp, CLIArguments);