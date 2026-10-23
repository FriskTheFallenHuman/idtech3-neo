execute_process(COMMAND "${BAKER}" "${FIXTURE}" "${OUTPUT}"
  RESULT_VARIABLE result OUTPUT_VARIABLE report ERROR_VARIABLE errors TIMEOUT 60)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Sprite baker failed: ${report}\n${errors}")
endif()
string(REGEX MATCH "PACK_ROOT=([^\r\n]+)" matched "${report}")
set(pack_root "${CMAKE_MATCH_1}")
if(NOT IS_DIRECTORY "${pack_root}")
  message(FATAL_ERROR "Baker did not produce a sprite pack")
endif()
get_filename_component(wad_directory "${IWAD}" DIRECTORY)
set(ENV{DOOMWADDIR} "${wad_directory}")
execute_process(COMMAND "${GAME}" -spritepacks "${pack_root}"
  -testhdsprites -testhidden -testresolution 640 480 -windowed -nosound -nomusic
  -warp 1 1 -frames 15 -capture "${OUTPUT}/game.ppm" -config "${OUTPUT}/game.cfg"
  WORKING_DIRECTORY "${OUTPUT}" RESULT_VARIABLE result OUTPUT_VARIABLE report ERROR_VARIABLE errors TIMEOUT 45)
file(WRITE "${OUTPUT}/game.log" "${report}\n${errors}")
if(NOT result EQUAL 0 OR NOT report MATCHES "HD sprite regression passed: 24 mappings" OR
   NOT report MATCHES "HD sprite rendering active" OR NOT EXISTS "${OUTPUT}/game.ppm")
  message(FATAL_ERROR "Runtime sprite test failed: ${result}\n${report}\n${errors}")
endif()
message(STATUS "FBX -> eight-angle PNG pack -> Doom state remapping -> GPU world rendering passed")
