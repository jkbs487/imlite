set(fileserver_SRCS
    TransferTask.cc
    FileServer.cc
    FileServer2.cc
    ../base/protobuf_codec.cc
    main.cc
)

add_executable(fileserver ${fileserver_SRCS})
target_link_libraries(fileserver slite_net protobuf_codec z im_proto uuid)
