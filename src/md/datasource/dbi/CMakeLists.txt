include(../../md_dbi.cmake)

add_precompiled_header(stdafx.h ../stdafx.cpp MDDATASOURCE_SOURCES)
add_library(mddatasource_dbi STATIC ${MDDATASOURCE_SOURCES})