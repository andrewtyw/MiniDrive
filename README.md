# MiniDrive
A web-based linux file server.

## Features
- 一个完全用`C++`实现的, 用于管理linux文件的Web服务。运行程序后, 用户登录成功用户即可看到其linux服务器某个文件夹下的所有文件; 用户可以删除/下载/上传文件;
## System design
- 使用`线程池`来调用线程处理工作队列中的事件, 比如连接事件, 信号事件, 读写事件。
- 使用`epoll`系列系统调用来实现`Reactor`模式的`IO`复用
- 使用有限状态机来解析`http request`, 构建`http response`和写入`http response`
- 使用管道进行系统信号 (e.g., `SIGALRM`) 的传递
- 基于升序链表, 设计了一个单例的定时器来**实现**了一个初级的`http session`
- 使用`session`和`Cookie`做验证, 从而用户的每次文件操作(e.g., 删除/下载文件) 一定程度上保证了web后端接口的安全性
- 实现了一个`半异步日志系统`, `写线程`从`队列`中取出内容，写入日志本地

## Set-up
- 推荐环境: Ubuntu 20.04, c++14, cmake: 3.24+
```shell
git clone git@github.com:Andrew-wong-ty/MiniDrive.git
cd MiniDrive/
mkdir build
cd build
cmake ..
make

# 运行主程序, 输入http:<你的外网ip>:8888/ 即可访问
# username=admin, password=123456
./main  
```

## In progress

1. ~~日志系统~~
2. 编码windows/Linux客户端来同步本地文件到远程Linux服务器

## Showcase

![](./utils/video.gif)

## Ref. & Acknowledgement
- 《Linux高性能服务器编程》游双
- WebFileServer: https://github.com/shangguanyongshi/WebFileServer
- TinyWebServer: https://github.com/qinguoyi/TinyWebServer
- MyPoorWebServer: https://github.com/forthespada/MyPoorWebServer
- linux-server: https://github.com/Hansimov/linux-server
- LinuxServerCodes: https://github.com/raichen/LinuxServerCodes