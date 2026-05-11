# User include paths
set(User_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/APP/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/LIB/Inc
)

# User BSP sources
set(User_BSP_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/dwt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/vofa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/ws2812.c
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/pwm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/mt6701.c
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/foc_adc.c
)

# User APP sources
set(User_APP_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/APP/Src/user_application.c
)

# User LIB sources
set(User_LIB_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/LIB/Src/foc.c
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
    target_link_libraries(User_APP PUBLIC stm32cubemx User_BSP)
else()
    add_library(User_APP INTERFACE)
    target_link_libraries(User_APP INTERFACE stm32cubemx User_BSP)
endif()

# Create User_LIB library (INTERFACE when no sources, OBJECT otherwise)
if(User_LIB_Src)
    add_library(User_LIB OBJECT)
    target_sources(User_LIB PRIVATE ${User_LIB_Src})
    target_link_libraries(User_LIB PUBLIC stm32cubemx User_BSP)
else()
    add_library(User_LIB INTERFACE)
    target_link_libraries(User_LIB INTERFACE stm32cubemx User_BSP)
endif()
