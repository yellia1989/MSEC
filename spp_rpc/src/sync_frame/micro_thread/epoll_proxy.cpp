
/**
 * Tencent is pleased to support the open source community by making MSEC available.
 *
 * Copyright (C) 2016 THL A29 Limited, a Tencent company. All rights reserved.
 *
 * Licensed under the GNU General Public License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. You may 
 * obtain a copy of the License at
 *
 *     https://opensource.org/licenses/GPL-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the 
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions
 * and limitations under the License.
 */


/**
 *  @filename epoll_proxy.cpp
 *  @info     epoll for micro thread manage
 */

#include "epoll_proxy.h"
#include "micro_thread.h"
#include "mt_monitor.h"

using namespace NS_MICRO_THREAD;

/**
 *  @brief 构造函数
 */
EpollProxy::EpollProxy()
{
    _maxfd = EpollProxy::DEFAULT_MAX_FD_NUM;
    _epfd = -1;
    _evtlist = NULL;
    _eprefs = NULL;
}

/**
 *  @brief epoll初始化, 申请动态内存等
 */
int EpollProxy::InitEpoll(int max_num)
{
    int rc = 0;
    if (max_num > _maxfd)   // 如果设置的数目较大, 则调整最大fd数目
    {
        _maxfd = max_num;
    }
    
    _epfd =  epoll_create(_maxfd);
    if (_epfd < 0)
    {
        rc = -1;
        goto EXIT_LABEL;
    }
    fcntl(_epfd, F_SETFD, FD_CLOEXEC);

    _eprefs = new FdRef[_maxfd];
    if (NULL == _eprefs)
    {
        rc = -2;
        goto EXIT_LABEL;
    }

    _evtlist = (EpEvent*)calloc(_maxfd, sizeof(EpEvent));
    if (NULL == _evtlist)
    {
        rc = -3;
        goto EXIT_LABEL;
    }
    
    struct rlimit rlim;
    memset(&rlim, 0, sizeof(rlim));
    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0)
    {
        if ((int)rlim.rlim_max < _maxfd)
        {
            rlim.rlim_cur = rlim.rlim_max;
            setrlimit(RLIMIT_NOFILE, &rlim);
            rlim.rlim_cur = _maxfd;
            rlim.rlim_max = _maxfd;
            setrlimit(RLIMIT_NOFILE, &rlim);
        } 
    }

EXIT_LABEL:

    if (rc < 0)
    {
        TermEpoll();
    }

    return rc;
}

/**
 *  @brief epoll反初始化
 */
void EpollProxy::TermEpoll()
{
    if (_epfd > 0)
    {
        close(_epfd);
        _epfd = -1;
    }
    
    if (_evtlist != NULL)
    {
        free(_evtlist);
        _evtlist = NULL;
    }
    
    if (_eprefs != NULL)
    {
        delete []_eprefs;
        _eprefs = NULL;
    }
}

/**
 *  @brief 将一个微线程侦听的所有socket送入epoll管理
 *  @param fdset 微线程侦听的socket集合
 *  @return true 成功, false 失败, 失败会尽力回滚, 减少影响
 */
bool EpollProxy::EpollAdd(EpObjList& obj_list)
{
    bool ret = true;
    EpollerObj *epobj = NULL;
    EpollerObj *epobj_error = NULL;
    TAILQ_FOREACH(epobj, &obj_list, _entry)
    {
        if (!EpollAddObj(epobj))
        {
            MTLOG_ERROR("epobj add failed, fd: %d", epobj->GetOsfd());
            epoll_assert(0);
            epobj_error = epobj;
            ret = false;
            goto EXIT_LABEL;
        }
    }

EXIT_LABEL:

    if (!ret)
    {
        TAILQ_FOREACH(epobj, &obj_list, _entry)
        {
            if (epobj == epobj_error)
            {
                break;
            }
            EpollDelObj(epobj);
        }
    }

    return ret;
}


/**
 *  @brief 将一个微线程侦听的所有socket移除epoll管理
 *  @param fdset 微线程侦听的socket集合
 *  @return true 成功, false 失败
 */
bool EpollProxy::EpollDel(EpObjList& obj_list)
{
    bool ret = true;
    
    EpollerObj *epobj = NULL;
    TAILQ_FOREACH(epobj, &obj_list, _entry)
    {
        if (!EpollDelObj(epobj))  // failed also need continue, be sure ref count ok
        {
            MTLOG_ERROR("epobj del failed, fd: %d", epobj->GetOsfd());
            epoll_assert(0);
            ret = false;
        }
    }

    return ret;
}


/**
 * @brief 单个epfd更新epctrl, 成功需要更新当前监听事件值
 */
bool EpollProxy::EpollCtrlAdd(int fd, int events)
{
    FdRef* item = FdRefGet(fd);
    if (NULL == item)
    {
        MT_ATTR_API(MONITOR_MT_EPOLL_FD_ERR, 1); // fd error
        MTLOG_ERROR("epfd ref not find, failed, fd: %d", fd);
        epoll_assert(0);
        return false;
    }

    // 更新引用计数, 部分流程会依赖该计数, 失败要回滚
    item->AttachEvents(events);
    
    int old_events = item->GetListenEvents();
    int new_events = old_events | events;
    if (old_events == new_events) {
        return true;
    }

    int op = old_events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    EpEvent ev;
    ev.events = new_events;
    ev.data.fd = fd;
    if ((epoll_ctl(_epfd, op, fd, &ev) < 0) && !(op == EPOLL_CTL_ADD && errno == EEXIST))
    {
        MT_ATTR_API(320850, 1); // epoll error
        MTLOG_ERROR("epoll ctrl failed, fd: %d, op: %d, errno: %d", fd, op, errno);
        item->DetachEvents(events);
        epoll_assert(0);
        return false;
    } 
    item->SetListenEvents(new_events);
    
    return true;

}

/**
 * @brief 单个epfd更新epctrl, 成功需要更新当前监听事件值
 */
bool EpollProxy::EpollCtrlDel(int fd, int events)
{
    return EpollCtrlDelRef(fd, events, false);
}

/**
 * @brief 单个epfd更新epctrl, 检查引用计数, 可以预设长连接, 不会每次都epollctl
 */
bool EpollProxy::EpollCtrlDelRef(int fd, int events, bool use_ref)
{
    FdRef* item = FdRefGet(fd);
    if (NULL == item)
    {
        MT_ATTR_API(MONITOR_MT_EPOLL_FD_ERR, 1); // fd error
        MTLOG_ERROR("epfd ref not find, failed, fd: %d", fd);
        epoll_assert(0);
        return false;
    }

    item->DetachEvents(events);  // delete 失败不回滚处理
    int old_events = item->GetListenEvents();
    int new_events = old_events &~ events;  // 默认情况

    // 如果要按引用删除, 需要核查是否满足删除条件
    if (use_ref)
    {
        new_events = old_events;
        if (0 == item->ReadRefCnt()) {
            new_events = new_events & ~EPOLLIN;
        }
        if (0 == item->WriteRefCnt()) {
            new_events = new_events & ~EPOLLOUT;
        }
    }

    if (old_events == new_events)
    {
        return true;
    }
    
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    EpEvent ev;
    ev.events = new_events;
    ev.data.fd = fd;
    if ((epoll_ctl(_epfd, op, fd, &ev) < 0) && !(op == EPOLL_CTL_DEL && errno == ENOENT))
    {
        MT_ATTR_API(320850, 1); // epoll error
        MTLOG_ERROR("epoll ctrl failed, fd: %d, op: %d, errno: %d", fd, op, errno);
        epoll_assert(0);
        return false;
    }    
    item->SetListenEvents(new_events);
    
    return true;
}



/**
 * @brief 单个epfd更新epctrl, 如果失败, 完整回退
 */
bool EpollProxy::EpollAddObj(EpollerObj* obj)
{
    if (NULL == obj)
    {
        MTLOG_ERROR("epobj input invalid, %p", obj);
        return false;
    }

    FdRef* item = FdRefGet(obj->GetOsfd());
    if (NULL == item)
    {    
        MT_ATTR_API(MONITOR_MT_EPOLL_FD_ERR, 1); // fd error
        MTLOG_ERROR("epfd ref not find, failed, fd: %d", obj->GetOsfd());
        epoll_assert(0);
        return false;
    }

    // 不同的回调状态, 不同的方式处理 del 事件, 屏蔽连接复用方式的处理复杂性
    int ret = obj->EpollCtlAdd(item);
    if (ret < 0)
    {
        MTLOG_ERROR("epoll ctrl callback failed, fd: %d, obj: %p", obj->GetOsfd(), obj);
        epoll_assert(0);
        return false;
    }

    return true;
}


/**
 *  @brief 将一个微线程侦听的所有socket移除epoll管理
 *  @param fdset 微线程侦听的socket集合
 *  @return true 成功, false 失败
 */
bool EpollProxy::EpollDelObj(EpollerObj* obj)
{
    if (NULL == obj)
    {
        MTLOG_ERROR("fdobj input invalid, %p", obj);
        return false;
    }
    
    FdRef* item = FdRefGet(obj->GetOsfd());
    if (NULL == item)
    {    
        MT_ATTR_API(MONITOR_MT_EPOLL_FD_ERR, 1); // fd error
        MTLOG_ERROR("epfd ref not find, failed, fd: %d", obj->GetOsfd());
        epoll_assert(0);
        return false;
    }
    
    // 不同的回调状态, 不同的方式处理 del 事件, 屏蔽连接复用方式的处理复杂性
    int ret = obj->EpollCtlDel(item);
    if (ret < 0)
    {
        MTLOG_ERROR("epoll ctrl callback failed, fd: %d, obj: %p", obj->GetOsfd(), obj);
        epoll_assert(0);
        return false;
    }

    return true;
}


/**
 *  @brief 更新每个socket的最新接收事件信息
 *  @param evtfdnum 收到事件的fd集合数目
 */
void EpollProxy::EpollRcvEventList(int evtfdnum)
{
    int ret = 0;
    int osfd = 0;
    int revents = 0;
    FdRef* item = NULL;
    EpollerObj* obj = NULL;

    for (int i = 0; i < evtfdnum; i++)
    {
        osfd = _evtlist[i].data.fd;
        item = FdRefGet(osfd);
        if (NULL == item)
        {
            MT_ATTR_API(MONITOR_MT_EPOLL_FD_ERR, 1); // fd error
            MTLOG_ERROR("epfd ref not find, failed, fd: %d", osfd);
            epoll_assert(0);
            continue;
        }
        
        revents = _evtlist[i].events;
        obj = item->GetNotifyObj();
        if (NULL == obj) 
        {
            MTLOG_ERROR("fd notify obj null, failed, fd: %d", osfd);
            EpollCtrlDel(osfd, (revents & (EPOLLIN | EPOLLOUT)));
            continue;
        }
        obj->SetRcvEvents(revents);

        // 1. 错误处理, 完毕后直接跳出
        if (revents & (EPOLLERR | EPOLLHUP))
        {
            obj->HangupNotify();
            continue;
        }

        // 2. 可读事件, 非0返回值会跳出
        if (revents & EPOLLIN) {
            ret = obj->InputNotify();
            if (ret != 0) {
                continue;
            }
        }

        // 3. 可写事件, 非0返回值会跳出
        if (revents & EPOLLOUT) {
            ret = obj->OutputNotify();
            if (ret != 0) {
                continue;
            }
        }
    }
}


/**
 *  @brief epoll_wait 以及分发处理过程
 */
void EpollProxy::EpollDispath()
{
    int wait_time = EpollGetTimeout();
    int nfd = epoll_wait(_epfd, _evtlist, _maxfd, wait_time);
    if (nfd <= 0) {
        return;
    }

    EpollRcvEventList(nfd);    
}


/**
 *  @brief 可读事件通知接口, 考虑通知处理可能会破坏环境, 可用返回值区分
 *  @return 0 该fd可继续处理其它事件; !=0 该fd需跳出回调处理
 */
int EpollerObj::InputNotify()
{
    MicroThread* thread = this->GetOwnerThread();
    if (NULL == thread) 
    {
        epoll_assert(0);
        MTLOG_ERROR("Epoll fd obj, no thread ptr, wrong");
        return -1;
    }

    // 多个事件同时到达, 防重复操作
    if (thread->HasFlag(MicroThread::IO_LIST))
    {
        MtFrame* frame = MtFrame::Instance();
        frame->RemoveIoWait(thread);
        frame->InsertRunable(thread);
    }

    return 0;    
}

/**
 *  @brief 可写事件通知接口, 考虑通知处理可能会破坏环境, 可用返回值区分
 *  @return 0 该fd可继续处理其它事件; !=0 该fd需跳出回调处理
 */
int EpollerObj::OutputNotify()
{
    MicroThread* thread = this->GetOwnerThread();
    if (NULL == thread) 
    {
        epoll_assert(0);
        MTLOG_ERROR("Epoll fd obj, no thread ptr, wrong");
        return -1;
    }

    // 多个事件同时到达, 防重复操作
    if (thread->HasFlag(MicroThread::IO_LIST))
    {
        MtFrame* frame = MtFrame::Instance();
        frame->RemoveIoWait(thread);
        frame->InsertRunable(thread);
    }

    return 0;    
}

/**
 *  @brief 异常通知接口, 关闭fd侦听, thread等待处理超时
 *  @return 忽略返回值, 跳过其它事件处理
 */
int EpollerObj::HangupNotify()
{
    MtFrame* frame = MtFrame::Instance();
    frame->EpollCtrlDel(this->GetOsfd(), this->GetEvents());
    return 0;
}

/**
 *  @brief 调整epoll侦听事件的回调接口, 长连接始终EPOLLIN, 偶尔EPOLLOUT
 *  @param args fd引用对象的指针
 *  @return 0 成功, < 0 失败, 要求事务回滚到操作前状态
 */
int EpollerObj::EpollCtlAdd(void* args)
{
    MtFrame* frame = MtFrame::Instance();
    FdRef* fd_ref = (FdRef*)args;
    epoll_assert(fd_ref != NULL);

    int osfd = this->GetOsfd();
    int new_events = this->GetEvents();

    // 通知对象需要更新, FD通知对象理论上不会复用, 这里做冲突检查, 异常log记录
    EpollerObj* old_obj = fd_ref->GetNotifyObj();
    if ((old_obj != NULL) && (old_obj != this))
    {
        MTLOG_ERROR("epfd ref conflict, fd: %d, old: %p, now: %p", osfd, old_obj, this);
        return -1;
    }
    fd_ref->SetNotifyObj(this);

    // 调用框架的epoll ctl接口, 屏蔽epoll ctrl细节
    if (!frame->EpollCtrlAdd(osfd, new_events))
    {
        MTLOG_ERROR("epfd ref add failed, log");
        fd_ref->SetNotifyObj(old_obj);
        return -2;
    }

    return 0;
}

/**
 *  @brief 调整epoll侦听事件的回调接口, 长连接始终EPOLLIN, 偶尔EPOLLOUT
 *  @param args fd引用对象的指针
 *  @return 0 成功, < 0 失败, 要求事务回滚到操作前状态
 */
int EpollerObj::EpollCtlDel(void* args)
{
    MtFrame* frame = MtFrame::Instance();
    FdRef* fd_ref = (FdRef*)args;
    epoll_assert(fd_ref != NULL);

    int osfd = this->GetOsfd();
    int events = this->GetEvents();
    
    // 通知对象需要更新, FD通知对象理论上不会复用, 这里做冲突检查, 异常log记录
    EpollerObj* old_obj = fd_ref->GetNotifyObj();
    if (old_obj != this)
    {
        MTLOG_ERROR("epfd ref conflict, fd: %d, old: %p, now: %p", osfd, old_obj, this);
        return -1;
    }
    fd_ref->SetNotifyObj(NULL);

    // 调用框架的epoll ctl接口, 屏蔽epoll ctrl细节
    if (!frame->EpollCtrlDelRef(osfd, events, false)) // 引用有风险, 弊大于利, 关闭掉
    {
        MTLOG_ERROR("epfd ref del failed, log");
        fd_ref->SetNotifyObj(old_obj);
        return -2;
    }

    return 0;
    
}


