set(COMPONENT_ADD_INCLUDEDIRS
    src
    )
file(GLOB SRCS
     src/*.c
     )
set(COMPONENT_SRCS ${SRCS})

register_component()
