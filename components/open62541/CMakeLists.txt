idf_component_register(
  SRCS "open62541.c"
  INCLUDE_DIRS "include"
)

target_include_directories(${COMPONENT_LIB} PRIVATE
    $ENV{IDF_PATH}/components/freertos/include
    $ENV{IDF_PATH}/components/freertos/FreeRTOS-Kernel/include
    $ENV{IDF_PATH}/components/freertos/FreeRTOS-Kernel/portable/xtensa/include
    $ENV{IDF_PATH}/components/freertos/FreeRTOS-Kernel/portable/xtensa/include/freertos
    $ENV{IDF_PATH}/components/lwip/include
    $ENV{IDF_PATH}/components/lwip/lwip/src/include
)