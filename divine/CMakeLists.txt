include( stringify )
#include( template-separe )

foreach( D mem vm ss mc ui cc ltl sim smt dbg ra )
    file( GLOB CPP_${D} ${D}/*.cpp )
    file( GLOB HPP_${D} ${D}/*.hpp )
    install( FILES ${HPP_${D}} DESTINATION include/divine/${D} )
endforeach()

file( GLOB H_BUILTIN LIST_DIRECTORIES false cc/*.h )
foreach( f ${H_BUILTIN} )
  stringify( cc ${divine_SOURCE_DIR}/divine/cc ${f} )
endforeach()
stringlist( "cc" cc )

#template_separe( VM vm/interpreter.cpp )

set( VERSION_FILE ${divine_BINARY_DIR}/divine/version-generated.cpp )
set( UPDATE_VERSION ${divine_SOURCE_DIR}/releng/update-version-sha.sh )

add_custom_target( divine-version
  COMMAND sh ${UPDATE_VERSION} "${SHA1SUM}" "${divine_SOURCE_DIR}" "${VERSION_FILE}" "${DIVINE_VERSION}" "${CMAKE_BUILD_TYPE}"
  # note: no DEPENDS to make target rebuild every time
  VERBATIM
  BYPRODUCTS ${VERSION_FILE}
  COMMENT "Calculate version SHA"
)

add_custom_command(
  OUTPUT flags-generated.cpp
  VERBATIM
  COMMAND sh -c "echo \"const char* divineCompileFlags = \\\"${BUILDOPTS}\\\";\">flags-generated.cpp"
)

include_directories( ${DIVINE_INCLUDES} ${CMAKE_BINARY_DIR} )
include_directories( SYSTEM ${DIVINE_SYS_INCLUDES} )
add_definitions( ${MPI_COMPILE_FLAGS} ${LLVM_COMPILE_FLAGS} )
add_definitions( ${DIVINE_DEFINES} )

set( CPP_ui ${CPP_ui} ${VERSION_FILE} "flags-generated.cpp" )
set( CPP_cc ${CPP_cc} ${cc_FILES} cc_list.cpp )

macro( mklib D )
  add_library( divine-${D} ${CPP_${D}} ${ARGN} )
  set_target_properties( divine-${D} PROPERTIES POSITION_INDEPENDENT_CODE ON )
  install( TARGETS divine-${D} DESTINATION lib )
endmacro()

foreach( D ltl mc smt vm dbg cc ui ra )
  mklib( ${D} )
endforeach()

add_dependencies( divine-ui divine-version )

if ( OPT_SIM )
  stringify( doc ${divine_SOURCE_DIR}/doc manual/sim.md )
  mklib( sim ${doc_FILES} )
  target_link_libraries( divine-sim ${LIBEDIT} ${LIBTINFO} divine-dbg )
  target_link_libraries( divine-ui divine-sim )
endif()

# target_compile_features( libdivine PUBLIC cxx_relaxed_constexpr )

bricks_unittest( test-divine ${HPP_ra} ${HPP_ss} ${HPP_mem} ${HPP_vm} ${HPP_mc} ${HPP_cc} ${HPP_ltl} ${HPP_smt} )

llvm_map_components_to_libnames( CC_TGTS ${LLVM_TARGETS_TO_BUILD} )
target_link_libraries( divine-cc LLVMCore LLVMSupport LLVMMC LLVMIRReader
                                 LLVMBitReader LLVMBitWriter LLVMLinker
                                 LLVMObject LLVMTransformUtils ${CC_TGTS}
                                 clang clangBasic clangCodeGen lldELF )
target_link_libraries( divine-smt ${Z3_LIBRARIES} ${STP_LIBRARIES} )
target_link_libraries( divine-dbg divine-vm )
target_link_libraries( divine-mc divine-vm divine-dbg divine-smt divine-rt divine-cc # FIXME divine-cc
                                 liblart LLVMBitReader LLVMBitWriter LLVMLinker )
target_link_libraries( divine-ui divine-rt divine-cc divine-mc divine-ltl divine-ra )
target_link_libraries( divine-ra divine-dbg divine-mc liblart )


if ( OPT_SQL )
    target_link_libraries( divine-ui nanodbc )
endif()

target_link_libraries( test-divine divine-cc divine-vm divine-ltl divine-dbg divine-mc divine-ra )

if( WIN32 )
  target_link_libraries( libdivine psapi )
endif()

add_custom_target( unit_divine
    COMMAND sh ${TEST_WRAPPER} ${WITH_LCOV} ${CMAKE_CURRENT_BINARY_DIR}/test-divine
    VERBATIM
    USES_TERMINAL
    DEPENDS test-divine )
add_dependencies( unit unit_divine )

install( FILES ${HPP} DESTINATION include/divine )
