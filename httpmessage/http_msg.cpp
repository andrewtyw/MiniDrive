#include "http_msg.h"

void Request::setRequestLine(const std::string &requestLine)
{
    std::istringstream lineStream(requestLine);
    // 获取请求方法
    lineStream >> requestMethod;
    // 获取请求资源
    lineStream >> rquestResourse;
    // 获取http版本
    lineStream >> httpVersion;
}

void Request::addHeaderOpt(const std::string &headLine)
{
    static std::istringstream lineStream;
    lineStream.str(headLine); // 以 istringstream 的方式处理头部选项

    std::string key, value; // 保存键和值的临时量

    lineStream >> key; // 获取 key
    key.pop_back();    // 删除键中的冒号
    lineStream.get();  // 删除冒号后的空格

    // 读取空格之后所有的数据，遇到 \n 停止，所以 value 中还包含一个 \r
    getline(lineStream, value);
    value.pop_back(); // 删除其中的 \r

    if (key == "Content-Length")
    {
        // 保存消息体的长度
        contentLength = std::stoll(value);
    }
    else if (key == "Content-Type")
    {
        // 分离消息体类型。消息体类型可能是复杂的消息体，类似 Content-Type: multipart/form-data; boundary=---------------------------24436669372671144761803083960

        // 先找出值中分号的位置
        std::string::size_type semIndex = value.find(';');
        // 根据分号查找的结果，保存类型的结果
        if (semIndex != std::string::npos)
        {
            msgHeader[key] = value.substr(0, semIndex);
            std::string::size_type eqIndex = value.find('=', semIndex);
            key = value.substr(semIndex + 2, eqIndex - semIndex - 2);
            msgHeader[key] = value.substr(eqIndex + 1);
        }
        else
        {
            msgHeader[key] = value;
        }
    }
    else if (key == "Cookie")
    {
        // 找出cookie 的 value
        // e.g. Cookie: userid=DJW498123ASD4FEQF4, key =userid, value = DJW498123ASD4FEQF4
        const static std::string userIdKey = "userid"; // userid的键值
        int startIndex = 0;
        int index;
        std::string cookieKey, cookieValue;
        for (index = 0; index < value.length() + 1; index++)
        {
            if (index >= value.length() || value[index] == ';' || value[index] == ' ')
            {
                if (cookieKey != "")
                    cookieValue = value.substr(startIndex, index - startIndex);
                if (cookieKey != "" && cookieValue != "")
                {
                    break;
                }
            }
            if (value[index] == '=')
            {
                cookieKey = value.substr(startIndex, index - startIndex);
                startIndex = index + 1;
            }
        }

        msgHeader[key] = cookieValue;
        LOG_DEBUG("在请求报文中接收到了cookie, cookie_key:%s,cookie_value:%s", cookieKey.c_str(), cookieValue.c_str());
    }
    else
    {
        msgHeader[key] = value;
    }
}