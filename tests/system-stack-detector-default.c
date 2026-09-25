/*
 ============================================================================
 Name        : system-stack-detector-default.c
 Description : Stack overflow detector default test
 ============================================================================
 */

/*
 * With the default configs.mk (ENABLE_STACK_OVERFLOW_DETECTOR := 0),
 * hev_task_system_init must leave the process-wide SIGSEGV and SIGBUS
 * handlers and the signal alternate stack as the host set them. The
 * detector's handler reads the task-system context of the faulting
 * thread, which threads that never ran hev_task_system_init (Go and Rust
 * threads in the same process) do not have.
 *
 * CI: only `make tests` in this repository builds and runs this file (and
 * that loop does not stop on a failure); the x-engine CI does not run it
 * until the tunnel adds a host-test target for it.
 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <hev-task-system.h>

/* The host's own handler: a fault anywhere in this test (for example
 * hev_task_system_fini destroying a detector that was never created) ends
 * it with a failure instead of re-entering the faulting instruction. */
static void
host_handler (int signo)
{
    static const char msg[] = "FAIL fault inside hev_task_system_*\n";
    ssize_t res;

    res = write (STDERR_FILENO, msg, sizeof (msg) - 1);
    (void)res;
    _exit (1);
}

static int
check_handler (int signo, const char *name)
{
    struct sigaction sa;

    if (sigaction (signo, NULL, &sa) < 0) {
        fprintf (stderr, "FAIL sigaction (%s) query\n", name);
        return 1;
    }
    if ((sa.sa_flags & SA_SIGINFO) || sa.sa_handler != host_handler) {
        fprintf (stderr,
                 "FAIL %s handler replaced by hev_task_system_init "
                 "(sa_flags=0x%x)\n",
                 name, (unsigned int)sa.sa_flags);
        return 1;
    }
    return 0;
}

int
main (int argc, char *argv[])
{
    struct sigaction sa;
    stack_t host_ss, ss;
    int failed = 0;

    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = host_handler;
    sigemptyset (&sa.sa_mask);
    if (sigaction (SIGSEGV, &sa, NULL) < 0 || sigaction (SIGBUS, &sa, NULL) < 0)
        return 2;
    if (sigaltstack (NULL, &host_ss) < 0)
        return 2;

    if (hev_task_system_init () < 0) {
        fprintf (stderr, "FAIL hev_task_system_init\n");
        return 1;
    }

    failed |= check_handler (SIGSEGV, "SIGSEGV");
    failed |= check_handler (SIGBUS, "SIGBUS");
    if (sigaltstack (NULL, &ss) < 0 || ss.ss_sp != host_ss.ss_sp ||
        ss.ss_size != host_ss.ss_size ||
        (ss.ss_flags & SS_DISABLE) != (host_ss.ss_flags & SS_DISABLE)) {
        fprintf (stderr, "FAIL alternate signal stack replaced\n");
        failed = 1;
    }

    hev_task_system_fini ();

    if (!failed)
        fprintf (stderr, "ok   stack overflow detector off by default\n");
    return failed;
}
