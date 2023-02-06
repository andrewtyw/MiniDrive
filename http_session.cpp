#include "http_session.h"

bool HttpSession::findAttribute(std::string findTarget)
{
    return (sessionItemsSet.find(findTarget) != sessionItemsSet.end());
}

std::vector<HttpSessionItem>::iterator HttpSession::getTargetIterator(std::string key)
{
    std::vector<HttpSessionItem>::iterator it;
    for (it = sessionItems.begin(); it != sessionItems.end(); it++)
    {
        if ((*it).key == key)
        {
            break;
        }
    }
    return it;
}

void HttpSession::removeAttribute(std::string key)
{
    if (findAttribute(key))
    {
        std::vector<HttpSessionItem>::iterator it;
        it = getTargetIterator(key);
        // 在vector和set中删除
        sessionItems.erase(it);
        if (sessionItemsSet.size() == 1)
            sessionItemsSet.clear();
        else
            sessionItemsSet.erase((*it).key);
        return;
    }
}

std::string HttpSession::getAttribute(std::string key)
{
    if (findAttribute(key))
    {
        std::vector<HttpSessionItem>::iterator it;
        it = getTargetIterator(key);
        return (*it).value;
    }
    return "";
}

void HttpSession::setAttribute(std::string key, std::string value)
{
    // 首先查找它是否存在, 如果存在, 先删除列表中的内容
    if (findAttribute(key))
    {
        std::vector<HttpSessionItem>::iterator it;
        it = getTargetIterator(key);
        sessionItems.erase(it);
    }

    HttpSessionItem sessionItem(key, value);
    // 已经按时间升序排好了, 当前时间为最大的时间戳, 因此直接插入到末尾
    sessionItems.push_back(sessionItem);
    sessionItemsSet.insert(key);
}

void HttpSession::tick()
{
    std::vector<HttpSessionItem>::iterator it;
    int64_t currentTime = HttpSessionItem::refreshTimeout() - HttpSessionItem::TIMEOUT_SLOT; // 要减回来
    std::cout << "tick! current time:" << currentTime << std::endl;
    for (it = sessionItems.begin(); it != sessionItems.end(); it++)
    {
        if ((*it).timeout < currentTime)
        {
            sessionItems.erase(it);
            if (sessionItemsSet.size() == 1)
                sessionItemsSet.clear();
            else
                sessionItemsSet.erase((*it).key);

            it = sessionItems.begin() - 1;
        }
        else
        {
            break;
        }
    }
}
