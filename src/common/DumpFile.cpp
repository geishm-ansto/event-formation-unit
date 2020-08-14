#include <common/DumpFile.h>

static hid_t MapPrimToH5tNative(H5PrimSubset Prim) {
  static hid_t PrimToH5tNative[static_cast<int>(H5PrimSubset::Count)] = {};
  static bool Init = true;
  if (Init) {
    Init = false;

    PrimToH5tNative[static_cast<int>(H5PrimSubset::Bool)] = H5T_NATIVE_HBOOL;

    PrimToH5tNative[static_cast<int>(H5PrimSubset::Float)] = H5T_NATIVE_FLOAT;

    PrimToH5tNative[static_cast<int>(H5PrimSubset::UInt64)] = H5T_NATIVE_ULLONG;
    static_assert(sizeof(uint64_t) == sizeof(unsigned long long),
                  "H5 types: Potential 32/64 bit issue.");

    PrimToH5tNative[static_cast<int>(H5PrimSubset::UInt32)] = H5T_NATIVE_UINT;
    static_assert(sizeof(uint32_t) == sizeof(unsigned int),
                  "H5 types: Potential 32/64 bit issue.");

    PrimToH5tNative[static_cast<int>(H5PrimSubset::UInt16)] = H5T_NATIVE_USHORT;
    static_assert(sizeof(uint16_t) == sizeof(unsigned short),
                  "Potential 32/64 bit definition issue.");

    PrimToH5tNative[static_cast<int>(H5PrimSubset::UInt8)] = H5T_NATIVE_UCHAR;
    static_assert(sizeof(uint8_t) == sizeof(unsigned char),
                  "H5 types: Potential 32/64 bit issue.");

    PrimToH5tNative[static_cast<int>(H5PrimSubset::SInt8)] = H5T_NATIVE_SCHAR;
    static_assert(sizeof(int8_t) == sizeof(signed char),
                  "H5 types: Potential 32/64 bit issue.");

    // verify all slots have been written
    for (int i = 0; i < static_cast<int>(H5PrimSubset::Count); i++) {
      if (PrimToH5tNative[i] == 0) {
        throw std::runtime_error(
            fmt::format("Missing H5PrimSubset to H5T_NATIVE_* mapping at "
                        "H5PrimSubset enum item {}",
                        i));
      }
    }
  }
  return PrimToH5tNative[static_cast<int>(Prim)];
}

PrimDumpFileBase::PrimDumpFileBase(const H5PrimCompoundDef &CompoundDef,
                                   const boost::filesystem::path &file_path,
                                   size_t max_Mb)
    : CompoundDef(CompoundDef),
      ChunkSize(
          9000 /
          CompoundDef.StructSize) // \todo 9000 is MTU? Correct size is <= 8972
                                  // else packet fragmentation will occur.
      ,
      Slab({0}, {ChunkSize}) {

  Compound = hdf5::datatype::Compound::create(CompoundDef.StructSize);

  for (size_t i = 0; i < CompoundDef.MembersCount; i++) {
    const H5PrimDef &Def = CompoundDef.Members[i];
    hid_t H5fNativeType = MapPrimToH5tNative(Def.PrimType);
    Compound.insert(
        Def.Name, Def.Offset,
        hdf5::datatype::Datatype(hdf5::ObjectHandle(H5Tcopy(H5fNativeType))));
  }

  MaxSize = max_Mb * 1000000 / CompoundDef.StructSize;
  PathBase = file_path;
}

std::unique_ptr<PrimDumpFileBase>
PrimDumpFileBase::create(const H5PrimCompoundDef &CompoundDef,
                         const boost::filesystem::path &FilePath,
                         size_t MaxMB) {
  auto Ret = std::unique_ptr<PrimDumpFileBase>(
      new PrimDumpFileBase(CompoundDef, FilePath, MaxMB));
  Ret->openRW();
  return Ret;
}

std::unique_ptr<PrimDumpFileBase>
PrimDumpFileBase::open(const H5PrimCompoundDef &CompoundDef,
                       const boost::filesystem::path &FilePath) {
  auto Ret = std::unique_ptr<PrimDumpFileBase>(
      new PrimDumpFileBase(CompoundDef, FilePath, 0));
  Ret->openR();
  return Ret;
}

size_t PrimDumpFileBase::count() const {
  return hdf5::dataspace::Simple(DataSet.dataspace())
      .current_dimensions()
      .at(0);
}

void PrimDumpFileBase::openRW() {
  using namespace hdf5;

  File = file::create(get_full_path(), file::AccessFlags::TRUNCATE);

  property::DatasetCreationList dcpl;
  dcpl.layout(property::DatasetLayout::CHUNKED);
  dcpl.chunk({ChunkSize});

  DataSet = File.root().create_dataset(
      CompoundDef.DatasetName, Compound,
      dataspace::Simple({0}, {dataspace::Simple::UNLIMITED}), dcpl);
  DataSet.attributes.create_from("format_version", CompoundDef.FormatVersion);
  DataSet.attributes.create_from("EFU_version", efu_version());
  DataSet.attributes.create_from("EFU_build_string", efu_buildstr());
}

void PrimDumpFileBase::openR() {
  using namespace hdf5;

  auto Path = PathBase;
  Path += ".h5";

  File = file::open(Path, file::AccessFlags::READONLY);
  DataSet = File.root().get_dataset(CompoundDef.DatasetName);
  // if not initial version, expect it to be well formed
  if (CompoundDef.FormatVersion > 0) {
    uint16_t ver{0};
    DataSet.attributes["format_version"].read(ver);
    if (ver != CompoundDef.FormatVersion) {
      throw std::runtime_error("DumpFile version mismatch");
    }
  }
}