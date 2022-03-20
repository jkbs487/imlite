set(dbproxyserver_SRCS
  EncDec.cc
  DBProxyServer.cc
  SyncCenter.cc
  models/DBPool.cc
  models/CachePool.cc
  models/DepartmentModel.cc
  models/UserModel.cc
  models/MessageModel.cc
  models/RelationModel.cc
  models/SessionModel.cc
  models/GroupModel.cc
  models/GroupMessageModel.cc
  models/FileModel.cc
)

SET(MYSQL_LIB /usr/lib64/mysql)
LINK_DIRECTORIES(${MYSQL_LIB})

add_executable(dbproxyserver ${dbproxyserver_SRCS})
target_link_libraries(dbproxyserver slite_net protobuf_codec im_proto mysqlclient hiredis ssl crypto z)