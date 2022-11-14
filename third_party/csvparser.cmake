# Homepage: https://sourceforge.net/projects/cccsvparser/
# Version: 2016_12_06

set(CSVPARSER_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/CsvParser)

add_library(csvparser
    ${CSVPARSER_ROOT}/src/csvparser.c)

target_include_directories(csvparser
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CSVPARSER_ROOT}/include>)
