# User include paths
set(User_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/APP/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/LIB/Inc
)

# User BSP sources
set(User_BSP_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/dwt.c
)

# User APP sources
set(User_APP_Src
)

# User LIB sources
set(User_LIB_Src
)

# Create User_BSP object library
add_library(User_BSP OBJECT)
target_sources(User_BSP PRIVATE ${User_BSP_Src})
target_include_directories(User_BSP PUBLIC ${User_Include_Dirs})
target_link_libraries(User_BSP PUBLIC stm32cubemx)

# Create User_APP library (INTERFACE when no sources, OBJECT otherwise)
if(User_APP_Src)
    add_library(User_APP OBJECT)
    target_sources(User_APP PRIVATE ${User_APP_Src})
else()
    add_library(User_APP INTERFACE)
endif()
target_link_libraries(User_APP INTERFACE stm32cubemx User_BSP)

# Create User_LIB library (INTERFACE when no sources, OBJECT otherwise)
if(User_LIB_Src)
    add_library(User_LIB OBJECT)
    target_sources(User_LIB PRIVATE ${User_LIB_Src})
else()
    add_library(User_LIB INTERFACE)
endif()
target_link_libraries(User_LIB INTERFACE stm32cubemx User_BSP)
