project(clashdometrn)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( clashdometrn clashdometrn clashdometrn.cpp )
target_include_directories( clashdometrn PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( clashdometrn ${CMAKE_SOURCE_DIR}/../ricardian )