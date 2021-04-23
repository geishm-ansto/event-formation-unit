# Create excecutable to test the Readout
set(AMReadoutTest_INC
  Readout.h
)
set(AMReadoutTest_SRC
  Readout.cpp
  ReadoutTest.cpp
)
create_test_executable(AMReadoutTest SKIP_MEMGRIND)

set(AMClusterTest_INC
  Readout.h
  EventBinning.h
  Cluster.h
  Clusterer.h
)
set(AMClusterTest_SRC
  Readout.cpp
  EventBinning.cpp
  Cluster.cpp
  Clusterer.cpp
  ClusterTest.cpp
)
create_test_executable(AMClusterTest SKIP_MEMGRIND)

set(AMEventBinningTest_INC
  Readout.h
  EventBinning.h
)
set(AMEventBinningTest_SRC
  Readout.cpp
  EventBinning.cpp
  EventBinningTest.cpp
)
create_test_executable(AMEventBinningTest SKIP_MEMGRIND)