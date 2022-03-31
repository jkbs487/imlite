set(loginserver_SRCS
    main.cc
    LoginServer.cc
    ../base/ConfigFileReader.cc
)

add_executable(loginserver ${loginserver_SRCS})
target_link_libraries(loginserver slite_net http protobuf_codec z im_proto)