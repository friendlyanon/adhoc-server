include("$ENV{UBPM_MODULE_PATH}" OPTIONAL RESULT_VARIABLE ubpm_available)

if(NOT ubpm_available)
  message(AUTHOR_WARNING "UBPM could not be included")
else()
  include(dependencies.cmake)
endif()
