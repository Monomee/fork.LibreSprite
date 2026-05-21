# Aseprite Network Library
# Copyright (C) 2001-2015 David Capello
# MonoSprite AI Extension (C) 2026 MonoSprite contributors

# Include path for third_party/json (header-only JSON library)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

add_library(net-lib
  http_request.cpp
  http_response.cpp
  ai_client.cpp
  ai_config.cpp)

target_link_libraries(net-lib
  base-lib
  ${CURL_LIBRARIES})
