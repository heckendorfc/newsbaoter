cmake_minimum_required (VERSION 2.6)
project (newsbaoter)

SET(CMAKE_C_FLAGS_DEBUG "-g -O0 -Wall")
SET(CMAKE_C_FLAGS_RELEASE "-O2")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

if (NOT DEFINED INSTALL_MANPREFIX)
	set(INSTALL_MANPREFIX ${CMAKE_INSTALL_PREFIX})
endif ()

if (NOT DEFINED INSTALL_LIBDIR)
	set(INSTALL_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib)
endif ()

add_definitions(-DPACKAGE_VERSION="0.1")
add_definitions(-DSHARE_PATH="${CMAKE_INSTALL_PREFIX}/share" )
add_definitions(-DLIB_PATH="${INSTALL_LIBDIR}" )


#libxslt
find_package (LibXml2 REQUIRED)
find_package (Threads REQUIRED)
find_package (CURL REQUIRED)
#find_package(Sqlite REQUIRED)
#include_directories(${SQLITE_INCLUDE_DIR})

add_executable (newsbaoter src/newsbaoter.c
src/config.c
src/hash.c
src/urlparse.c
)
#src/xmlproc.c

target_link_libraries (newsbaoter ${CMAKE_THREAD_LIBS_INIT})

#FILE(GLOB shared_files share/*)
SET(shared_files share/defaults.conf share/create.sql)
SET(doc_files README)
SET(man_files share/newsbaoter.1)
INSTALL(FILES ${shared_files} DESTINATION share/newsbaoter)
INSTALL(FILES ${man_files} DESTINATION ${INSTALL_MANPREFIX}/man/man1)
INSTALL(FILES ${doc_files} DESTINATION share/doc/newsbaoter)
INSTALL(TARGETS newsbaoter RUNTIME DESTINATION bin)