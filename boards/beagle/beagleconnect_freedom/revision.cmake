set(BOARD_REVISIONS "c5" "c7")
if(NOT DEFINED BOARD_REVISION)
  set(BOARD_REVISION "c7")
else()
  if(NOT BOARD_REVISION IN_LIST BOARD_REVISIONS)
    message(FATAL_ERROR "${BOARD_REVISION} is not a valid revision for beagleconnect_freedom. Accepted revisions: ${BOARD_REVISIONS}")
  endif()
endif()

