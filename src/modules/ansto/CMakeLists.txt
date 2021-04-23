include_directories(.)

#=============================================================================
# Detector Pipeline Plugins
#=============================================================================
set(ansto_common_inc
  AnstoBase.h
  common/Readout.h
  common/Config.h
  common/ParserFactory.h
  common/EventBinning.h
  common/Cluster.h
  common/Clusterer.h
  mcpd8/DataParser.h
  bnlp/BnlpParser.h
  bnlpz/BnlpzParser.h
)
set(ansto_common_src
  AnstoBase.cpp
  common/Readout.cpp
  common/Config.cpp
  common/ParserFactory.cpp
  common/EventBinning.cpp
  common/Cluster.cpp
  common/Clusterer.cpp
  mcpd8/DataParser.cpp
  bnlp/BnlpParser.cpp
  bnlpz/BnlpzParser.cpp
)

set(ansto_INC
  ${ansto_common_inc}
)
set(ansto_SRC
  Ansto.cpp
  ${ansto_common_src}
)
create_module(ansto)

get_filename_component(TEST_JSON_PATH "configs/testsim.json" DIRECTORY)
set(TEST_JSON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_JSON_PATH}/")

add_subdirectory(common)
add_subdirectory(mcpd8)
add_subdirectory(bnlp)
add_subdirectory(bnlpz)
add_subdirectory(generators)