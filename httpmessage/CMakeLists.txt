aux_source_directory(. SRC_MSG_LIST)
# 生成链接库
add_library (http_msg ${SRC_MSG_LIST})