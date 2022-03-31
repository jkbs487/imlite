set(msgserver_SRCS
    main.cc
    MsgServer.cc
    DBProxyClient.cc
    LoginClient.cc
    RouteClient.cc
    FileClient.cc
    ImUser.cc
    ../base/protobuf_codec.cc
    ../base/ConfigFileReader.cc
)

add_executable(msgserver ${msgserver_SRCS})
target_link_libraries(msgserver slite_net protobuf_codec z im_proto)