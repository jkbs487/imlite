set(fileserver_SRCS
    FileServer.cc
)

add_executable(fileserver ${fileserver_SRCS})
target_link_libraries(fileserver slite_net protobuf_codec z im_proto)
