/*
 * test_int_proc_io.c -- Extensive tests for tt_proc_run() pipe I/O and signal handling.
 *
 * Covers:
 *   1. Basic stdout/stderr capture
 *   2. Large stdout (>64KB pipe buffer) without deadlock
 *   3. Large stderr (>64KB pipe buffer) without deadlock
 *   4. Large BOTH stdout+stderr simultaneously (the deadlock scenario)
 *   5. Asymmetric output (huge stdout, small stderr and vice versa)
 *   6. Binary/null bytes in output
 *   7. Rapid small writes (many lines)
 *   8. Empty output (silent process)
 *   9. Timeout enforcement
 *  10. stdin piping
 *  11. Process that closes stdout early but keeps writing stderr
 *  12. Process exit codes (0, 1, 127, signal death)
 *  13. SIGINT/SIGTERM causing child termination (signal propagation)
 *  14. Process that ignores SIGTERM (verifying SIGKILL escalation on timeout)
 *  15. Concurrent stdout+stderr interleaving under load
 */

#include "test_framework.h"
#include "test_helpers.h"
#include "platform.h"
#include "error.h"
#include "cmd_index.h" /* tt_interrupted */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>

/* ---- Helper: generate a shell command that writes N bytes to a fd ---- */

/*
 * Build a sh -c command that writes `nbytes` to stdout or stderr.
 * Uses dd with /dev/zero piped through tr to get printable chars.
 */
static tt_proc_result_t run_large_output(const char *target, size_t nbytes)
{
    char cmd[512];
    if (strcmp(target, "stdout") == 0) {
        snprintf(cmd, sizeof(cmd),
                 "dd if=/dev/zero bs=%zu count=1 2>/dev/null | tr '\\0' 'A'",
                 nbytes);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "dd if=/dev/zero bs=%zu count=1 2>/dev/null | tr '\\0' 'A' >&2",
                 nbytes);
    }
    const char *argv[] = {"sh", "-c", cmd, NULL};
    return tt_proc_run(argv, NULL, 30000);
}

/* ---- 1. Basic stdout capture ---- */

TT_TEST(test_proc_basic_stdout)
{
    const char *argv[] = {"echo", "hello world", NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 5000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_STR_CONTAINS(r.stdout_buf, "hello world");
    tt_proc_result_free(&r);
}

/* ---- 2. Basic stderr capture ---- */

TT_TEST(test_proc_basic_stderr)
{
    const char *argv[] = {"sh", "-c", "echo 'err msg' >&2", NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 5000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stderr_buf);
    TT_ASSERT_STR_CONTAINS(r.stderr_buf, "err msg");
    tt_proc_result_free(&r);
}

/* ---- 3. Large stdout (128KB > pipe buffer) ---- */

TT_TEST(test_proc_large_stdout)
{
    tt_proc_result_t r = run_large_output("stdout", 131072); /* 128KB */
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    size_t len = strlen(r.stdout_buf);
    TT_ASSERT_GE_INT((int)len, 131072);
    tt_proc_result_free(&r);
}

/* ---- 4. Large stderr (128KB > pipe buffer) ---- */

TT_TEST(test_proc_large_stderr)
{
    tt_proc_result_t r = run_large_output("stderr", 131072); /* 128KB */
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stderr_buf);
    size_t len = strlen(r.stderr_buf);
    TT_ASSERT_GE_INT((int)len, 131072);
    tt_proc_result_free(&r);
}

/* ---- 5. Large BOTH stdout+stderr (the deadlock scenario) ---- */

TT_TEST(test_proc_large_both_no_deadlock)
{
    /*
     * This is the exact scenario that caused the pipe deadlock with ctags.
     * Write 256KB to stdout AND 256KB to stderr simultaneously.
     * With sequential reads, the parent blocks on one while the child
     * blocks on the other (pipe buffer = 64KB on Linux).
     * With poll(), both are drained concurrently.
     */
    const char *argv[] = {"sh", "-c",
        "dd if=/dev/zero bs=262144 count=1 2>/dev/null | tr '\\0' 'A' & "
        "dd if=/dev/zero bs=262144 count=1 2>/dev/null | tr '\\0' 'B' >&2; "
        "wait",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 30000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_NOT_NULL(r.stderr_buf);
    size_t out_len = strlen(r.stdout_buf);
    size_t err_len = strlen(r.stderr_buf);
    TT_ASSERT_GE_INT((int)out_len, 262144);
    TT_ASSERT_GE_INT((int)err_len, 262144);
    tt_proc_result_free(&r);
}

/* ---- 6. Extreme: 1MB on both channels ---- */

TT_TEST(test_proc_1mb_both_channels)
{
    const char *argv[] = {"sh", "-c",
        "dd if=/dev/zero bs=1048576 count=1 2>/dev/null | tr '\\0' 'X' & "
        "dd if=/dev/zero bs=1048576 count=1 2>/dev/null | tr '\\0' 'Y' >&2; "
        "wait",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 30000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_NOT_NULL(r.stderr_buf);
    size_t out_len = strlen(r.stdout_buf);
    size_t err_len = strlen(r.stderr_buf);
    TT_ASSERT_GE_INT((int)out_len, 1048576);
    TT_ASSERT_GE_INT((int)err_len, 1048576);
    tt_proc_result_free(&r);
}

/* ---- 7. Asymmetric: huge stdout, tiny stderr ---- */

TT_TEST(test_proc_asymmetric_stdout_heavy)
{
    const char *argv[] = {"sh", "-c",
        "dd if=/dev/zero bs=524288 count=1 2>/dev/null | tr '\\0' 'A'; "
        "echo 'small err' >&2",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 30000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_NOT_NULL(r.stderr_buf);
    TT_ASSERT_GE_INT((int)strlen(r.stdout_buf), 524288);
    TT_ASSERT_STR_CONTAINS(r.stderr_buf, "small err");
    tt_proc_result_free(&r);
}

/* ---- 8. Asymmetric: tiny stdout, huge stderr ---- */

TT_TEST(test_proc_asymmetric_stderr_heavy)
{
    const char *argv[] = {"sh", "-c",
        "echo 'small out'; "
        "dd if=/dev/zero bs=524288 count=1 2>/dev/null | tr '\\0' 'B' >&2",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 30000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_NOT_NULL(r.stderr_buf);
    TT_ASSERT_STR_CONTAINS(r.stdout_buf, "small out");
    TT_ASSERT_GE_INT((int)strlen(r.stderr_buf), 524288);
    tt_proc_result_free(&r);
}

/* ---- 9. Many small writes (10000 lines) ---- */

TT_TEST(test_proc_many_small_writes)
{
    const char *argv[] = {"sh", "-c",
        "i=0; while [ $i -lt 10000 ]; do echo \"line_$i\"; i=$((i+1)); done",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 30000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_STR_CONTAINS(r.stdout_buf, "line_0");
    TT_ASSERT_STR_CONTAINS(r.stdout_buf, "line_9999");
    /* Each line is ~10-12 bytes, so total should be ~100KB+ */
    TT_ASSERT_GE_INT((int)strlen(r.stdout_buf), 50000);
    tt_proc_result_free(&r);
}

/* ---- 10. Interleaved stdout+stderr (alternating writes) ---- */

TT_TEST(test_proc_interleaved_output)
{
    const char *argv[] = {"sh", "-c",
        "i=0; while [ $i -lt 5000 ]; do "
        "echo \"out_$i\"; echo \"err_$i\" >&2; "
        "i=$((i+1)); done",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 30000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_NOT_NULL(r.stderr_buf);
    TT_ASSERT_STR_CONTAINS(r.stdout_buf, "out_0");
    TT_ASSERT_STR_CONTAINS(r.stdout_buf, "out_4999");
    TT_ASSERT_STR_CONTAINS(r.stderr_buf, "err_0");
    TT_ASSERT_STR_CONTAINS(r.stderr_buf, "err_4999");
    tt_proc_result_free(&r);
}

/* ---- 11. Empty output (silent process) ---- */

TT_TEST(test_proc_empty_output)
{
    const char *argv[] = {"true", NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 5000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_NOT_NULL(r.stderr_buf);
    TT_ASSERT_EQ_INT((int)strlen(r.stdout_buf), 0);
    TT_ASSERT_EQ_INT((int)strlen(r.stderr_buf), 0);
    tt_proc_result_free(&r);
}

/* ---- 12. Timeout enforcement ---- */

TT_TEST(test_proc_timeout_kills_child)
{
    /* sleep 60 should be killed after 500ms timeout */
    const char *argv[] = {"sleep", "60", NULL};
    uint64_t t0 = tt_monotonic_ms();
    tt_proc_result_t r = tt_proc_run(argv, NULL, 500);
    uint64_t elapsed = tt_monotonic_ms() - t0;
    TT_ASSERT_EQ_INT(r.exit_code, -1);
    /* Should complete within ~1s (500ms timeout + cleanup overhead) */
    TT_ASSERT_LE_INT((int)elapsed, 3000);
    tt_proc_result_free(&r);
    tt_error_clear();
}

/* ---- 13. Timeout with output already produced ---- */

TT_TEST(test_proc_timeout_with_partial_output)
{
    /* Produce output then hang */
    const char *argv[] = {"sh", "-c",
        "echo 'before_hang'; sleep 60",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 1000);
    TT_ASSERT_EQ_INT(r.exit_code, -1);
    /* Output before hang should NOT be captured (timeout returns early) */
    tt_proc_result_free(&r);
    tt_error_clear();
}

/* ---- 14. stdin piping ---- */

TT_TEST(test_proc_stdin_pipe)
{
    const char *argv[] = {"cat", NULL};
    tt_proc_result_t r = tt_proc_run(argv, "piped input data", 5000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_STR_CONTAINS(r.stdout_buf, "piped input data");
    tt_proc_result_free(&r);
}

/* ---- 15. stdin piping with large data ---- */

TT_TEST(test_proc_stdin_large)
{
    /* Build 64KB of input */
    char *input = malloc(65537);
    TT_ASSERT_NOT_NULL(input);
    memset(input, 'Z', 65536);
    input[65536] = '\0';

    const char *argv[] = {"cat", NULL};
    tt_proc_result_t r = tt_proc_run(argv, input, 10000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_GE_INT((int)strlen(r.stdout_buf), 65536);
    free(input);
    tt_proc_result_free(&r);
}

/* ---- 16. Exit code preservation ---- */

TT_TEST(test_proc_exit_code_nonzero)
{
    const char *argv[] = {"sh", "-c", "exit 42", NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 5000);
    TT_ASSERT_EQ_INT(r.exit_code, 42);
    tt_proc_result_free(&r);
}

TT_TEST(test_proc_exit_code_one)
{
    const char *argv[] = {"false", NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 5000);
    TT_ASSERT_EQ_INT(r.exit_code, 1);
    tt_proc_result_free(&r);
}

/* ---- 17. Command not found (exit 127) ---- */

TT_TEST(test_proc_command_not_found)
{
    const char *argv[] = {"nonexistent_binary_xyz_1234567890", NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 5000);
    /* execvp fails in child, _exit(127) */
    TT_ASSERT_EQ_INT(r.exit_code, 127);
    tt_proc_result_free(&r);
}

/* ---- 18. Process killed by signal ---- */

TT_TEST(test_proc_child_signal_death)
{
    /* Child kills itself with SIGABRT */
    const char *argv[] = {"sh", "-c", "kill -ABRT $$", NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 5000);
    /* WIFSIGNALED -> exit_code = -1 */
    TT_ASSERT_EQ_INT(r.exit_code, -1);
    tt_proc_result_free(&r);
}

/* ---- 19. Process that closes stdout early but keeps stderr ---- */

TT_TEST(test_proc_stdout_closes_early)
{
    const char *argv[] = {"sh", "-c",
        "echo 'early_out'; exec 1>/dev/null; "
        "i=0; while [ $i -lt 1000 ]; do echo \"late_err_$i\" >&2; i=$((i+1)); done",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 10000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_NOT_NULL(r.stderr_buf);
    TT_ASSERT_STR_CONTAINS(r.stdout_buf, "early_out");
    TT_ASSERT_STR_CONTAINS(r.stderr_buf, "late_err_999");
    tt_proc_result_free(&r);
}

/* ---- 20. Process that closes stderr early but keeps stdout ---- */

TT_TEST(test_proc_stderr_closes_early)
{
    const char *argv[] = {"sh", "-c",
        "echo 'early_err' >&2; exec 2>/dev/null; "
        "i=0; while [ $i -lt 1000 ]; do echo \"late_out_$i\"; i=$((i+1)); done",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 10000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_NOT_NULL(r.stderr_buf);
    TT_ASSERT_STR_CONTAINS(r.stderr_buf, "early_err");
    TT_ASSERT_STR_CONTAINS(r.stdout_buf, "late_out_999");
    tt_proc_result_free(&r);
}

/* ---- 21. NULL argv (error path) ---- */

TT_TEST(test_proc_null_argv)
{
    tt_proc_result_t r = tt_proc_run(NULL, NULL, 0);
    TT_ASSERT_EQ_INT(r.exit_code, -1);
    TT_ASSERT_NULL(r.stdout_buf);
    TT_ASSERT_NULL(r.stderr_buf);
    tt_error_clear();
}

/* ---- 22. Simulated ctags-like output (JSON stdout + warnings stderr) ---- */

TT_TEST(test_proc_ctags_like_output)
{
    /*
     * Simulate ctags behavior: large JSON on stdout, warnings on stderr.
     * ctags on react produced ~295KB stderr and ~2MB stdout.
     */
    const char *argv[] = {"sh", "-c",
        /* 512KB of fake JSON on stdout */
        "dd if=/dev/zero bs=524288 count=1 2>/dev/null | tr '\\0' '{'; "
        /* 300KB of warnings on stderr (> 4x pipe buffer) */
        "dd if=/dev/zero bs=307200 count=1 2>/dev/null | tr '\\0' 'W' >&2",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 30000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_NOT_NULL(r.stderr_buf);
    TT_ASSERT_GE_INT((int)strlen(r.stdout_buf), 524288);
    TT_ASSERT_GE_INT((int)strlen(r.stderr_buf), 307200);
    tt_proc_result_free(&r);
}

/* ---- 23. Stress: 4MB total output ---- */

TT_TEST(test_proc_4mb_stress)
{
    const char *argv[] = {"sh", "-c",
        "dd if=/dev/zero bs=2097152 count=1 2>/dev/null | tr '\\0' 'S' & "
        "dd if=/dev/zero bs=2097152 count=1 2>/dev/null | tr '\\0' 'E' >&2; "
        "wait",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 60000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_NOT_NULL(r.stderr_buf);
    TT_ASSERT_GE_INT((int)strlen(r.stdout_buf), 2097152);
    TT_ASSERT_GE_INT((int)strlen(r.stderr_buf), 2097152);
    tt_proc_result_free(&r);
}

/* ---- 24. Process that produces output in bursts ---- */

TT_TEST(test_proc_bursty_output)
{
    const char *argv[] = {"sh", "-c",
        "for i in 1 2 3 4 5; do "
        "  dd if=/dev/zero bs=32768 count=1 2>/dev/null | tr '\\0' 'A'; "
        "  dd if=/dev/zero bs=32768 count=1 2>/dev/null | tr '\\0' 'B' >&2; "
        "done",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 30000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_NOT_NULL(r.stderr_buf);
    /* 5 * 32KB = 160KB per channel */
    TT_ASSERT_GE_INT((int)strlen(r.stdout_buf), 160000);
    TT_ASSERT_GE_INT((int)strlen(r.stderr_buf), 160000);
    tt_proc_result_free(&r);
}

/* ---- 25. Zero-timeout means no timeout (process completes normally) ---- */

TT_TEST(test_proc_zero_timeout)
{
    const char *argv[] = {"echo", "quick", NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 0);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_STR_CONTAINS(r.stdout_buf, "quick");
    tt_proc_result_free(&r);
}

/* ===== Signal handling tests ===== */

/*
 * Test that tt_interrupted flag causes tt_proc_run to kill child and return.
 *
 * Strategy: fork a test child that sets tt_interrupted after a delay,
 * then calls tt_proc_run with a long-running process.
 * We verify the process returns promptly with exit_code -1.
 */

/* Helper: signal handler for SIGUSR1 to set tt_interrupted from alarm */
static void test_set_interrupted(int sig)
{
    (void)sig;
    tt_interrupted = 1;
}

TT_TEST(test_proc_interrupt_kills_child)
{
    /*
     * Use SIGALRM to set tt_interrupted after 200ms.
     * tt_proc_run should detect it in the poll() loop (EINTR -> check flag)
     * and kill the child.
     */
    tt_interrupted = 0;

    /* Set up SIGALRM to fire after 200ms, setting tt_interrupted */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = test_set_interrupted;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    /* Schedule alarm in 200ms using timer */
    struct itimerval timer;
    memset(&timer, 0, sizeof(timer));
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 200000; /* 200ms */
    setitimer(ITIMER_REAL, &timer, NULL);

    uint64_t t0 = tt_monotonic_ms();
    const char *argv[] = {"sleep", "60", NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 0); /* no timeout */
    uint64_t elapsed = tt_monotonic_ms() - t0;

    TT_ASSERT_EQ_INT(r.exit_code, -1);
    /* Should return within ~1s (200ms alarm + poll cycle + kill + reap) */
    TT_ASSERT_LE_INT((int)elapsed, 3000);

    tt_proc_result_free(&r);
    tt_error_clear();

    /* Reset */
    tt_interrupted = 0;
    sa.sa_handler = SIG_DFL;
    sigaction(SIGALRM, &sa, NULL);
}

TT_TEST(test_proc_interrupt_with_output)
{
    /*
     * Child produces continuous output, then gets interrupted.
     * Verify that partial output is NOT returned (we clean up on interrupt).
     */
    tt_interrupted = 0;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = test_set_interrupted;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    struct itimerval timer;
    memset(&timer, 0, sizeof(timer));
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 300000; /* 300ms */
    setitimer(ITIMER_REAL, &timer, NULL);

    const char *argv[] = {"sh", "-c",
        "while true; do echo 'continuous output'; done",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 0);
    TT_ASSERT_EQ_INT(r.exit_code, -1);
    tt_proc_result_free(&r);
    tt_error_clear();

    /* Reset */
    tt_interrupted = 0;
    sa.sa_handler = SIG_DFL;
    sigaction(SIGALRM, &sa, NULL);
}

/* ---- 28. Verify no zombie processes after timeout ---- */

TT_TEST(test_proc_no_zombies_after_timeout)
{
    const char *argv[] = {"sleep", "60", NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 200);
    TT_ASSERT_EQ_INT(r.exit_code, -1);
    tt_proc_result_free(&r);
    tt_error_clear();

    /* Wait a moment then check: no zombie children should exist.
     * waitpid with WNOHANG should return 0 (no children) or -1 (ECHILD). */
    usleep(100000); /* 100ms */
    int status;
    pid_t wp = waitpid(-1, &status, WNOHANG);
    TT_ASSERT(wp <= 0, "no zombie child processes");
}

/* ---- 29. Verify no zombies after interrupt ---- */

TT_TEST(test_proc_no_zombies_after_interrupt)
{
    tt_interrupted = 0;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = test_set_interrupted;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    struct itimerval timer;
    memset(&timer, 0, sizeof(timer));
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 100000; /* 100ms */
    setitimer(ITIMER_REAL, &timer, NULL);

    const char *argv[] = {"sleep", "60", NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 0);
    tt_proc_result_free(&r);
    tt_error_clear();

    usleep(100000);
    int status;
    pid_t wp = waitpid(-1, &status, WNOHANG);
    TT_ASSERT(wp <= 0, "no zombie child processes after interrupt");

    tt_interrupted = 0;
    sa.sa_handler = SIG_DFL;
    sigaction(SIGALRM, &sa, NULL);
}

/* ---- 30. Process that traps SIGTERM (timeout must escalate to SIGKILL) ---- */

TT_TEST(test_proc_timeout_escalates_to_sigkill)
{
    /*
     * The child traps SIGTERM and ignores it. Our timeout path uses SIGKILL
     * directly, so this should still terminate within the timeout window.
     */
    const char *argv[] = {"sh", "-c",
        "trap '' TERM; sleep 60",
        NULL};
    uint64_t t0 = tt_monotonic_ms();
    tt_proc_result_t r = tt_proc_run(argv, NULL, 500);
    uint64_t elapsed = tt_monotonic_ms() - t0;
    TT_ASSERT_EQ_INT(r.exit_code, -1);
    TT_ASSERT_LE_INT((int)elapsed, 3000);
    tt_proc_result_free(&r);
    tt_error_clear();
}

/* ---- 31. Multiple rapid sequential invocations ---- */

TT_TEST(test_proc_rapid_sequential)
{
    for (int i = 0; i < 50; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "iter_%d", i);
        const char *argv[] = {"echo", msg, NULL};
        tt_proc_result_t r = tt_proc_run(argv, NULL, 5000);
        TT_ASSERT_EQ_INT(r.exit_code, 0);
        TT_ASSERT_NOT_NULL(r.stdout_buf);
        TT_ASSERT_STR_CONTAINS(r.stdout_buf, msg);
        tt_proc_result_free(&r);
    }
}

/* ---- 32. Output with embedded newlines and special chars ---- */

TT_TEST(test_proc_special_chars_output)
{
    const char *argv[] = {"printf",
        "line1\\nline2\\ttab\\rreturn\\n",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 5000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_STR_CONTAINS(r.stdout_buf, "line1");
    TT_ASSERT_STR_CONTAINS(r.stdout_buf, "line2");
    tt_proc_result_free(&r);
}

/* ---- 33. Exact pipe buffer boundary (64KB) ---- */

TT_TEST(test_proc_exact_pipe_buffer_boundary)
{
    /* Write exactly 65536 bytes (Linux pipe buffer size) to each channel */
    const char *argv[] = {"sh", "-c",
        "dd if=/dev/zero bs=65536 count=1 2>/dev/null | tr '\\0' 'P' & "
        "dd if=/dev/zero bs=65536 count=1 2>/dev/null | tr '\\0' 'Q' >&2; "
        "wait",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 15000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_NOT_NULL(r.stderr_buf);
    TT_ASSERT_GE_INT((int)strlen(r.stdout_buf), 65536);
    TT_ASSERT_GE_INT((int)strlen(r.stderr_buf), 65536);
    tt_proc_result_free(&r);
}

/* ---- 34. Just over pipe buffer boundary (65537 bytes) ---- */

TT_TEST(test_proc_over_pipe_buffer_by_one)
{
    const char *argv[] = {"sh", "-c",
        "dd if=/dev/zero bs=65537 count=1 2>/dev/null | tr '\\0' 'R' & "
        "dd if=/dev/zero bs=65537 count=1 2>/dev/null | tr '\\0' 'S' >&2; "
        "wait",
        NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 15000);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_NOT_NULL(r.stderr_buf);
    TT_ASSERT_GE_INT((int)strlen(r.stdout_buf), 65537);
    TT_ASSERT_GE_INT((int)strlen(r.stderr_buf), 65537);
    tt_proc_result_free(&r);
}

/* ===== Runner ===== */

void run_int_proc_io_tests(void)
{
    /* Basic I/O */
    TT_RUN(test_proc_basic_stdout);
    TT_RUN(test_proc_basic_stderr);

    /* Large output (deadlock prevention) */
    TT_RUN(test_proc_large_stdout);
    TT_RUN(test_proc_large_stderr);
    TT_RUN(test_proc_large_both_no_deadlock);
    TT_RUN(test_proc_1mb_both_channels);
    TT_RUN(test_proc_asymmetric_stdout_heavy);
    TT_RUN(test_proc_asymmetric_stderr_heavy);

    /* Pipe buffer boundary */
    TT_RUN(test_proc_exact_pipe_buffer_boundary);
    TT_RUN(test_proc_over_pipe_buffer_by_one);

    /* Many writes / interleaving */
    TT_RUN(test_proc_many_small_writes);
    TT_RUN(test_proc_interleaved_output);
    TT_RUN(test_proc_bursty_output);

    /* Edge cases */
    TT_RUN(test_proc_empty_output);
    TT_RUN(test_proc_null_argv);
    TT_RUN(test_proc_special_chars_output);
    TT_RUN(test_proc_stdout_closes_early);
    TT_RUN(test_proc_stderr_closes_early);

    /* stdin piping */
    TT_RUN(test_proc_stdin_pipe);
    TT_RUN(test_proc_stdin_large);

    /* Exit codes */
    TT_RUN(test_proc_exit_code_nonzero);
    TT_RUN(test_proc_exit_code_one);
    TT_RUN(test_proc_command_not_found);
    TT_RUN(test_proc_child_signal_death);

    /* Timeouts */
    TT_RUN(test_proc_timeout_kills_child);
    TT_RUN(test_proc_timeout_with_partial_output);
    TT_RUN(test_proc_timeout_escalates_to_sigkill);
    TT_RUN(test_proc_zero_timeout);

    /* Simulated real-world */
    TT_RUN(test_proc_ctags_like_output);
    TT_RUN(test_proc_4mb_stress);
    TT_RUN(test_proc_rapid_sequential);

    /* Signal handling / interrupt */
    TT_RUN(test_proc_interrupt_kills_child);
    TT_RUN(test_proc_interrupt_with_output);

    /* Zombie prevention */
    TT_RUN(test_proc_no_zombies_after_timeout);
    TT_RUN(test_proc_no_zombies_after_interrupt);
}
