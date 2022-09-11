set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")

set(includes "include")

set(COMPONENT_SRCS
  src/config.c
  src/echonet.c
  src/packet.c
  src/socket.c
  src/object.c
  src/nodeprofile.c
)

idf_component_register(SRCS ${COMPONENT_SRCS} INCLUDE_DIRS ${includes})
