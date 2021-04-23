# Create excecutable to generate raw UDP packets for the ansto server

set(udpgen_ansto_INC
  igenbase.h
  mcpd8.h
  bnlpz.h
  ../common/Config.h
)
set(udpgen_ansto_SRC
  udpgen_ansto.cpp
  mcpd8.cpp
  bnlpz.cpp
  ../common/Config.cpp
)
create_executable(udpgen_ansto)

set(test_zmq_INC
)
set(test_zmq_SRC
  test_zmq.cpp
)
create_executable(test_zmq)

set(AMGenMcpd8Test_INC
  mcpd8.h
  ../mcpd8/DataParser.h
  ../common/Readout.h
  ../common/Config.h
  ../common/ParserFactory.h
)
set(AMGenMcpd8Test_SRC
  mcpd8.cpp
  ../mcpd8/DataParser.cpp
  ../common/Readout.cpp
  ../common/Config.cpp
  ../common/ParserFactory.cpp
  GenMcpd8Test.cpp
)
create_test_executable(AMGenMcpd8Test SKIP_MEMGRIND)

target_compile_definitions(AMGenMcpd8Test PRIVATE TEST_JSON_PATH="${TEST_JSON_PATH}/")