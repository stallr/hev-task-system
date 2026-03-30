/*
 ============================================================================
 Name        : hev-task-system.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2025 everyone.
 Description :
 ============================================================================
 */

#include <stdlib.h>
#include <pthread.h>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include "lib/misc/hev-compiler.h"
#include "lib/misc/hev-task-stack-detector.h"
#include "mem/api/hev-memory-allocator-api.h"
#include "mem/slice/hev-memory-allocator-slice.h"

#include "hev-task-system-private.h"

#include "hev-task-system.h"

static pthread_key_t key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

static void
pthread_key_creator (void)
{
    pthread_key_create (&key, NULL);
}

#ifdef _WIN32
static int
hev_task_system_prepare_windows_fiber (HevTaskSystemContext *context)
{
    if (IsThreadAFiber ()) {
        context->kernel_fiber = GetCurrentFiber ();
        context->owns_kernel_fiber = 0;
        return 0;
    }

    context->kernel_fiber =
        ConvertThreadToFiberEx (NULL, FIBER_FLAG_FLOAT_SWITCH);
    if (!context->kernel_fiber)
        return -1;

    context->owns_kernel_fiber = 1;
    return 0;
}
#endif

HevTaskSystemContext *
hev_task_system_get_context (void)
{
    return pthread_getspecific (key);
}

static inline int
hev_task_system_set_context (HevTaskSystemContext *context)
{
    return pthread_setspecific (key, context);
}

EXPORT_SYMBOL int
hev_task_system_init (void)
{
    HevMemoryAllocator *allocator = NULL;
    HevTaskSystemContext *context;

#ifdef ENABLE_MEMALLOC_SLICE
    allocator = hev_memory_allocator_slice_new ();
#endif
    allocator = hev_memory_allocator_set_default (allocator);
    if (allocator)
        hev_memory_allocator_unref (allocator);

    pthread_once (&key_once, pthread_key_creator);

    if (hev_task_system_get_context ())
        goto exit;

    context = hev_malloc0 (sizeof (HevTaskSystemContext));
    if (!context)
        goto exit;

    if (hev_task_system_set_context (context) < 0)
        goto free_context;

    context->reactor = hev_task_io_reactor_new ();
    if (!context->reactor)
        goto rest_context;

    context->timer = hev_task_timer_new (context);
    if (!context->timer)
        goto free_reactor;

    context->stack_detector = hev_task_stack_detector_new ();
    if (!context->stack_detector)
        goto free_timer;

#ifdef _WIN32
    if (hev_task_system_prepare_windows_fiber (context) < 0)
        goto free_stack_detector;
#endif

    return 0;

#ifdef _WIN32
free_stack_detector:
    hev_task_stack_detector_destroy (context->stack_detector);
#endif
free_timer:
    hev_task_timer_destroy (context->timer);
free_reactor:
    hev_task_io_reactor_destroy (context->reactor);
rest_context:
    hev_task_system_set_context (NULL);
free_context:
    hev_free (context);
exit:
    return -1;
}

EXPORT_SYMBOL void
hev_task_system_fini (void)
{
    HevMemoryAllocator *allocator;
    HevTaskSystemContext *context = hev_task_system_get_context ();

    if (context->dns_proxy)
        hev_task_dns_proxy_destroy (context->dns_proxy);
    hev_task_stack_detector_destroy (context->stack_detector);
    hev_task_timer_destroy (context->timer);
    hev_task_io_reactor_destroy (context->reactor);
#ifdef _WIN32
    if (context->owns_kernel_fiber)
        ConvertFiberToThread ();
#endif
    hev_free (context);
    hev_task_system_set_context (NULL);

    allocator = hev_memory_allocator_set_default (NULL);
    if (allocator)
        hev_memory_allocator_unref (allocator);
}

EXPORT_SYMBOL void
hev_task_system_run (void)
{
    hev_task_system_schedule (HEV_TASK_RUN_SCHEDULER);
}
