/*
 ============================================================================
 Name        : hev-windows-api.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2025 everyone.
 Description : Windows API
 ============================================================================
 */

#ifndef __HEV_WINDOWS_API_H__
#define __HEV_WINDOWS_API_H__

/*
 * Windows SDK headers already provide the socket/event/IOCP declarations we
 * need. Reuse them for both MinGW and MSVC-style builds so the types keep the
 * correct calling convention and dllimport attributes.
 */
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <stdint.h>
#include <winsock2.h>
#include <windows.h>
#include <io.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef INFINITE
#define INFINITE 0xffffffff
#endif

#ifndef FD_READ
#define FD_READ (1 << 0)
#endif

#ifndef FD_WRITE
#define FD_WRITE (1 << 1)
#endif

#ifndef FD_ACCEPT
#define FD_ACCEPT (1 << 3)
#endif

#ifndef FD_CONNECT
#define FD_CONNECT (1 << 4)
#endif

#ifndef FD_CLOSE
#define FD_CLOSE (1 << 5)
#endif

static inline intptr_t
hev_windows_fd_to_wait_handle (int fd)
{
    int sock_type = 0;
    int sock_type_len = sizeof (sock_type);
    SOCKET sock = (SOCKET)(uintptr_t)fd;

    if (fd < 0)
        return -1;

    if (getsockopt (sock, SOL_SOCKET, SO_TYPE, (char *)&sock_type,
                    &sock_type_len) == 0)
        return (intptr_t)sock;

    return _get_osfhandle (fd);
}

#ifdef __cplusplus
}
#endif

#endif /* __HEV_WINDOWS_API_H__ */
