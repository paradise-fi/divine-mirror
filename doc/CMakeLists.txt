find_program( PANDOC pandoc )
find_program( CONTEXT context )
find_program( PANDOC_CROSSREF pandoc-crossref )

set( MANUAL_SRC overview.md building.md using.md commands.md llvm.md sim.md divm.md dios.md )
set( MANUAL_OPT "--filter=${PANDOC_CROSSREF} -MlinkReferences:true -MsecPrefix:Section -N --toc" )

if ( PANDOC AND PANDOC_CROSSREF )
  if ( NOT CONTEXT )
    message( WARNING "context not found, skipping pdf generation" )
  endif()
  execute_process(COMMAND ${PANDOC} - -o - -f markdown -t latex --smart
                  RESULT_VARIABLE PANDOC_SMART_RET
                  INPUT_FILE /dev/null
                  OUTPUT_QUIET ERROR_QUIET )
  if ( PANDOC_SMART_RET EQUAL 0 )
      set( PANDOC_SMART_OPT --smart )
  else()
      set( PANDOC_SMART_OPT --from=markdown+smart+spaced_reference_links )
  endif()
  add_subdirectory( manual )
  add_subdirectory( website )
else()
  message( WARNING "cound not find pandoc or pandoc-crossref, not building documentation" )
endif()

