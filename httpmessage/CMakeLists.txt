aux_source_directory(. SRC_HTTPMSG_LIST)
# 生成链接库
add_library (http_msg ${SRC_HTTPMSG_LIST})