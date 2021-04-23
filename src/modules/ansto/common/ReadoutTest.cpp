/** Copyright (C) 2016, 2017 European Spallation Source ERIC */

#include "Readout.h"
#include <test/TestBase.h>

using namespace Ansto;

class AnstoReaderTest : public TestBase {
protected:
  void SetUp() override {
    hdf5::error::Singleton::instance().auto_print(false);
    if (boost::filesystem::exists("hit_file_test_00000.h5")) {
      boost::filesystem::remove("hit_file_test_00000.h5");
    }
  }
  void TearDown() override {}
};

TEST_F(AnstoReaderTest, PrintsSelf) {
  Readout h;
  EXPECT_FALSE(h.debug().empty());
  // Don't really care about particular contents here
}

//   uint64_t timestamp {0};
//   uint8_t event_type {0};
//   uint8_t source {0};
//   uint16_t x_posn {0};
//   uint16_t y_posn {0};
//   uint16_t weight {0};
//   uint32_t data {0};

TEST_F(AnstoReaderTest, CompoundMapping) {
  // If you are forced to change anything here,
  // you have broken dumpfile compatibility, and you should
  // bump FormatVersion for the struct

  auto t = hdf5::datatype::create<Readout>();

  EXPECT_EQ(t.number_of_fields(), 7ul);
  EXPECT_EQ(t.get_class(), hdf5::datatype::Class::COMPOUND);

  auto ct = hdf5::datatype::Compound(t);

  EXPECT_EQ(ct.field_name(0), "timestamp");
  EXPECT_EQ(ct[0], hdf5::datatype::create<uint64_t>());

  EXPECT_EQ(ct.field_name(1), "event_type");
  EXPECT_EQ(ct[1], hdf5::datatype::create<uint8_t>());

  EXPECT_EQ(ct.field_name(2), "source");
  EXPECT_EQ(ct[2], hdf5::datatype::create<uint8_t>());

  EXPECT_EQ(ct.field_name(3), "x_posn");
  EXPECT_EQ(ct[3], hdf5::datatype::create<uint16_t>());

  EXPECT_EQ(ct.field_name(4), "y_posn");
  EXPECT_EQ(ct[4], hdf5::datatype::create<uint16_t>());

  EXPECT_EQ(ct.field_name(5), "weight");
  EXPECT_EQ(ct[5], hdf5::datatype::create<uint16_t>());

  EXPECT_EQ(ct.field_name(6), "data");
  EXPECT_EQ(ct[6], hdf5::datatype::create<uint32_t>());
}

TEST_F(AnstoReaderTest, CreateFile) {
  ReadoutFile::create("hit_file_test");
  EXPECT_TRUE(hdf5::file::is_hdf5_file("hit_file_test_00000.h5"));
}

TEST_F(AnstoReaderTest, CheckStringMap) {
  auto &map = Ansto::StringToEventType;
  EXPECT_EQ(map.at("Neutron"), Neutron);
  EXPECT_EQ(map.at("FrameStart"), FrameStart);
  EXPECT_EQ(map.at("FrameAuxStart"), FrameAuxStart);
  EXPECT_EQ(map.at("FrameDeassert"), FrameDeassert);
  EXPECT_EQ(map.at("Veto"), Veto);
  EXPECT_EQ(map.at("BeamMonitor"), BeamMonitor);

  try {
    EXPECT_EQ(map.at("NeutronX"), Neutron);
  } catch (std::out_of_range &err) {
    EXPECT_EQ(err.what(), std::string("map::at"));
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
