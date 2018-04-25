/** Copyright (C) 2018 European Spallation Source ERIC */

/** @file
 *
 *  @brief Sample processing of the ADC data.
 */

#include "SampleProcessing.h"
#include "senv_data_generated.h"
#include "AdcReadoutConstants.h"
#include <cmath>

std::uint64_t CalcSampleTimeStamp(const RawTimeStamp &Start,
                                  const RawTimeStamp &End,
                                  const TimeStampLocation Location) {
  std::uint64_t StartNS = Start.GetTimeStampNS();
  std::uint64_t EndNS = End.GetTimeStampNS();
  if (TimeStampLocation::Start == Location) {
    return StartNS;
  }
  if (TimeStampLocation::End == Location) {
    return EndNS;
  }
  return StartNS + (EndNS - StartNS) / 2;
}

double CalcTimeStampDelta(int OversamplingFactor) {
  constexpr double SampleTime = 1.0 / AdcTimerCounterMax;
  return SampleTime * OversamplingFactor;
}

ProcessedSamples ChannelProcessing::processModule(const DataModule &Samples) {
  int FinalOversamplingFactor = MeanOfNrOfSamples * Samples.OversamplingFactor;
  size_t SampleIndex{0};
  size_t TotalNumberOfSamples = (Samples.Data.size() + NrOfSamplesSummed) / MeanOfNrOfSamples;
  ProcessedSamples ReturnSamples(TotalNumberOfSamples);
  
  ReturnSamples.TimeDelta = CalcTimeStampDelta(FinalOversamplingFactor);
  std::uint64_t TimeStampOffset{0};
  if (TSLocation == TimeStampLocation::Middle) {
    TimeStampOffset = std::llround(0.5 * (1e9*ReturnSamples.TimeDelta  / FinalOversamplingFactor) * (FinalOversamplingFactor - 1));
  } else if (TSLocation == TimeStampLocation::End) {
    TimeStampOffset = std::llround((1e9*ReturnSamples.TimeDelta  / FinalOversamplingFactor) * (FinalOversamplingFactor - 1));
  }
  
  for (size_t i = 0; i < Samples.Data.size(); i++) {
    if (0 == NrOfSamplesSummed) {
      TimeStampOfFirstSample = Samples.TimeStamp.GetOffsetTimeStamp(
          i * Samples.OversamplingFactor - (Samples.OversamplingFactor - 1));
    }
    SumOfSamples += Samples.Data[i];
    NrOfSamplesSummed++;
    if (NrOfSamplesSummed == MeanOfNrOfSamples) {
      ReturnSamples.Samples[SampleIndex] = SumOfSamples / FinalOversamplingFactor;
        ReturnSamples.TimeStamps[SampleIndex] = TimeStampOfFirstSample.GetTimeStampNS() + TimeStampOffset;

      ChannelProcessing::reset();
      ++SampleIndex;
    }
  }
  if (not ReturnSamples.TimeStamps.empty()) {
    ReturnSamples.TimeStamp = ReturnSamples.TimeStamps[0];
  }
  ReturnSamples.Channel = Samples.Channel;
  return ReturnSamples;
}

void ChannelProcessing::setMeanOfSamples(int NrOfSamples) {
  MeanOfNrOfSamples = NrOfSamples;
  reset();
}

void ChannelProcessing::setTimeStampLocation(TimeStampLocation Location) {
  TSLocation = Location;
}

void ChannelProcessing::reset() {
  SumOfSamples = 0;
  NrOfSamplesSummed = 0;
}

SampleProcessing::SampleProcessing(std::shared_ptr<ProducerBase> Prod,
                                   std::string const &Name)
    : AdcDataProcessor(std::move(Prod)), AdcName(Name) {}

void SampleProcessing::setMeanOfSamples(int NrOfSamples) {
  MeanOfNrOfSamples = NrOfSamples;
  for (auto &Processor : ProcessingInstances) {
    Processor.second.setMeanOfSamples(NrOfSamples);
  }
}

void SampleProcessing::setSerializeTimestamps(bool SerializeTimeStamps) {
  SampleTimestamps = SerializeTimeStamps;
}

void SampleProcessing::setTimeStampLocation(TimeStampLocation Location) {
  TSLocation = Location;
  for (auto &Processor : ProcessingInstances) {
    Processor.second.setTimeStampLocation(Location);
  }
}

void SampleProcessing::processPacket(const PacketData &Data) {
  for (auto &Module : Data.Modules) {
    if (ProcessingInstances.find(Module.Channel) == ProcessingInstances.end()) {
      ProcessingInstances[Module.Channel] = ChannelProcessing();
      setMeanOfSamples(MeanOfNrOfSamples);
      setTimeStampLocation(TSLocation);
    }
    auto ResultingSamples = ProcessingInstances[Module.Channel].processModule(Module);
    if (not ResultingSamples.Samples.empty()) {
      serializeAndTransmitData(ResultingSamples);
    }
  }
}

void SampleProcessing::serializeAndTransmitData(ProcessedSamples const &Data) {
  flatbuffers::FlatBufferBuilder builder;
  auto FBSampleData = builder.CreateVector(Data.Samples);
  flatbuffers::Offset<flatbuffers::Vector<std::uint64_t>> FBTimeStamps;
  if (SampleTimestamps) {
    FBTimeStamps = builder.CreateVector(Data.TimeStamps);
  }

  auto FBName =
      builder.CreateString(AdcName + "_" + std::to_string(Data.Channel));
  SampleEnvironmentDataBuilder MessageBuilder(builder);
  MessageBuilder.add_Name(FBName);
  MessageBuilder.add_Values(FBSampleData);
  if (SampleTimestamps) {
    MessageBuilder.add_Timestamps(FBTimeStamps);
  }
  MessageBuilder.add_Channel(Data.Channel);
  MessageBuilder.add_PacketTimestamp(Data.TimeStamp);
  MessageBuilder.add_TimeDelta(Data.TimeDelta);

  // Note: std::map zero initialises new elements when using the [] operator
  MessageBuilder.add_MessageCounter(MessageCounters[Data.Channel]++);
  MessageBuilder.add_TimestampLocation(
      Location(TimeLocSerialisationMap.at(TSLocation)));
  builder.Finish(MessageBuilder.Finish(), SampleEnvironmentDataIdentifier());
  ProducerPtr->produce(reinterpret_cast<char *>(builder.GetBufferPointer()),
                       builder.GetSize());
}
