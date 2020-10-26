#pragma once
#include "common/Detector.h"
#include <asio.hpp>
#include "common/Producer.h"
#include "../adc_readout/EventSerializer.h"


class Timestamp : public Detector {
public:
  Timestamp(BaseSettings const &Settings);
  ~Timestamp() = default;

  void stopThreads() override;
private:
  void threadFunc();
  BaseSettings Settings;
  std::shared_ptr<asio::io_service> Service;
  asio::io_service::work Worker;
  Producer KafkaProducer;
  EventSerializer Serializer;
};

