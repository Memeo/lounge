CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(LoungeAct)
set(LoungeAct_VERSION_MAJOR 0)
set(LoungeAct_VERSION_MINOR 0)
set(LoungeAct_VERSION_MICRO 1)

add_subdirectory(jansson)

if ("$ENV{BDB_PREFIX}" STREQUAL "")
    set(BDB_LIBS "")
    set(BDB_INCLUDES "")
else ("$ENV{BDB_PREFIX}" STREQUAL "")
    set(BDB_LIBS "$ENV{BDB_PREFIX}/lib")
    set(BDB_INCLUDES "$ENV{BDB_PREFIX}/include")
endif ("$ENV{BDB_PREFIX}" STREQUAL "")

find_library(BDB db ${BDB_LIBS})
find_library(SQLITE3 sqlite3)

set(LoungeAct_SOURCES utils/buffer.c utils/hexdump.c utils/stringutils.c)
set(LoungeAct_SOURCES ${LoungeAct_SOURCES} compress-lz4/compress-lz4.c 
    compress-lz4/lz4/lz4.c)
set(LoungeAct_SOURCES ${LoungeAct_SOURCES} codec-json/codec-json.c)
set(LoungeAct_SOURCES ${LoungeAct_SOURCES} Storage/ObjectStore.c)
if (HAVE_BERKELEYDB)
    set(LoungeAct_SOURCES ${LoungeAct_SOURCES} bdb-storage/ObjectStore-BDB.c)
    set(BDB_LINK_LIBS db)
    include_directories(${BDB_INCLUDES})
endif (HAVE_BERKELEYDB)
if (HAVE_SQLITE3)
    set(LoungeAct_SOURCES ${LoungeAct_SOURCES} sqlite-storage/ObjectStore-sqlite.c)
    set(SQLITE_LINK_LIBS sqlite3)
endif (HAVE_SQLITE3)
set(LoungeAct_SOURCES ${LoungeAct_SOURCES} revgen/revgen.c revgen-couch/couch-revgen.c)
set(LoungeAct_SOURCES ${LoungeAct_SOURCES} api/api.c)
set(LoungeAct_SOURCES ${LoungeAct_SOURCES} pull/pull.c)
set(LoungeAct_SOURCES ${LoungeAct_SOURCES} push/push.c)

include_directories(.)
include_directories(utils)
include_directories(compress-lz4)
include_directories(compress-lz4/lz4)
include_directories(jansson/src)
include_directories(api)

add_library(loungeact SHARED ${LoungeAct_SOURCES})
target_link_libraries(loungeact jansson curl ${SQLITE3} ${BDB})