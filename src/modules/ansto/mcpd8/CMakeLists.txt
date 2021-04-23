# Create excecutable to test the parser
set(AMDataParserTest_INC
  DataParser.h
  ../common/Readout.h
  ../common/Config.h
  ../common/ParserFactory.h)
set(AMDataParserTest_SRC
  DataParser.cpp
  ../common/Readout.cpp
  ../common/Config.cpp
  ../common/ParserFactory.cpp
  DataParserTest.cpp)
create_test_executable(AMDataParserTest)