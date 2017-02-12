
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
 *  @filename mt_sys_hook.h
 *  @info  微线程hook系统api, 以不用额外编译的优势, 转同步为异步库
 *         HOOK 部分, 参考pth与libco实现
 */

#ifndef _MT_SYS_HOOK___
#define _MT_SYS_HOOK___

#include <poll.h>
#include <dlfcn.h>

#ifdef  __cplusplus
extern "C" {
#endif

/******************************************************************************/
/*         1. HOOK 的函数定义部分                                             */
/******************************************************************************/

typedef int (*func_socket)(int domain, int type, int protocol);
typedef int (*func_close)(int fd);
typedef int (*func_connect)(int socket, const struct sockaddr *address, socklen_t address_len);
typedef int (*func_accept)(int socket, struct sockaddr *address, socklen_t *addrlen);
typedef ssize_t (*func_read)(int fildes, void *buf, size_t nbyte);
typedef ssize_t (*func_write)(int fildes, const void *buf, size_t nbyte);
typedef ssize_t (*func_sendto)(int socket, const void *message, size_t length, 
                        int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
typedef ssize_t (*func_recvfrom)(int socket, void *buffer, size_t length,
	                    int flags, struct sockaddr *address, socklen_t *address_len);
typedef size_t (*func_send)(int socket, const void *buffer, size_t length, int flags);
typedef ssize_t (*func_recv)(int socket, void *buffer, size_t length, int flags);
typedef int (*func_select)(int nfds, fd_set *readfds, fd_set *writefds,
                        fd_set *exceptfds, struct timeval *timeout);
typedef int (*func_poll)(struct pollfd fds[], nfds_t nfds, int timeout);
typedef int (*func_setsockopt)(int socket, int level, int option_name,
			            const void *option_value, socklen_t option_len);
typedef int (*func_fcntl)(int fildes, int cmd, ...);
typedef int (*func_ioctl)(int fildes, int request, ... );

typedef unsigned int (*func_sleep)(unsigned int seconds);			            


/******************************************************************************/
/*         2.  全局的hook函数结构                                             */
/******************************************************************************/

/**
 * @brief Hook的原始函数集中管理定义, 支持动态新增处理
 */ 
typedef struct mt_syscall_func_tab
{
    func_socket             real_socket;
    func_close              real_close;
    func_connect            real_connect;
    func_read               real_read;
    func_write              real_write;
    func_sendto             real_sendto;
    func_recvfrom           real_recvfrom;
    func_send               real_send;
    func_recv               real_recv;
    func_setsockopt         real_setsockopt;
    func_fcntl              real_fcntl;
    func_ioctl              real_ioctl;
    
    func_sleep              real_sleep;             // 暂不支持，因为没有与fd关联, 防止滥用
    func_select             real_select;            // 暂不支持, 1024限制问题
    func_poll               real_poll;              // 暂不支持, 确认需求后实施

    func_accept             real_accept;
}MtSyscallFuncTab;


/******************************************************************************/
/*         3.  直接调用原始系统api的接口                                      */
/******************************************************************************/
extern MtSyscallFuncTab  g_mt_syscall_tab;            // 全局符号表
extern int               g_mt_hook_flag;              // 全局控制标记

#define mt_hook_syscall(name)                                                  \
do  {                                                                          \
        if (!g_mt_syscall_tab.real_##name) {                                   \
           g_mt_syscall_tab.real_##name = (func_##name)dlsym(RTLD_NEXT, #name);\
        }                                                                      \
    } while (0)

#define mt_real_func(name)      g_mt_syscall_tab.real_##name

#define mt_set_hook_flag()      (g_mt_hook_flag = 1)
#define mt_unset_hook_flag()    (g_mt_hook_flag = 0)

#define mt_hook_active()        (g_mt_hook_flag == 1)



#ifdef  __cplusplus
}
#endif

#endif


