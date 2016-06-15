/**
 * @file landslide-aec.c
 * @brief Dummy implementation of Landslide, our (sadly proprietary) model checker
 */

#define _GNU_SOURCE

#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/******************************************************************************
 * spec
 ******************************************************************************/

#define MESSAGE_BUF_SIZE 256

struct output_message {
	unsigned int magic;

	enum {
		THUNDERBIRDS_ARE_GO = 0,
		DATA_RACE = 1,
		ESTIMATE = 2,
		FOUND_A_BUG = 3,
		SHOULD_CONTINUE = 4,
		ASSERT_FAILED = 5,
	} tag;

	union {
		struct {
			unsigned int eip;
			unsigned int tid;
			unsigned int last_call;
			unsigned int most_recent_syscall;
			bool confirmed;
			bool deterministic;
			bool free_re_malloc;
			char pretty_printed[MESSAGE_BUF_SIZE];
		} dr;

		struct {
			long double proportion;
			unsigned int elapsed_branches;
			long double total_usecs;
			long double elapsed_usecs;
			unsigned int icb_cur_bound;
		} estimate;

		struct {
			char trace_filename[MESSAGE_BUF_SIZE];
			unsigned int icb_preemption_count;
		} bug;

		struct {
			char assert_message[MESSAGE_BUF_SIZE];
		} crash_report;
	} content;
};

struct input_message {
	unsigned int magic;
	enum {
		SHOULD_CONTINUE_REPLY = 0,
		SUSPEND_TIME = 1,
		RESUME_TIME = 2,
	} tag;
	bool value;
};

/******************************************************************************
 * globals
 ******************************************************************************/

int input_fd, output_fd, log_fd;
bool use_pipes;
unsigned int message_magic;

/******************************************************************************
 * glue
 ******************************************************************************/

void message_assert_fail(const char *, const char *, unsigned int, const char *);

#ifdef assert
#undef assert
#endif
#define assert(expr) do { if (!(expr)) {                                \
		message_assert_fail(__STRING(expr), __FILE__,         \
				      __LINE__, __func__);     \
	} } while (0)

#define FAIL_PRE_INIT(msg) do {					\
		char *__msg = (msg);				\
		int tty_fd = open("/dev/tty", O_WRONLY);	\
		if (tty_fd < 0) exit(8);			\
		write(tty_fd, __msg, strlen(__msg));		\
		exit(1);					\
	} while (0)

void send(struct output_message *m)
{
	if (use_pipes) {
		m->magic = message_magic;
		int ret = write(output_fd, m, sizeof(struct output_message));
		assert(ret == sizeof(struct output_message) && "write failed");
	} else if (m->tag == THUNDERBIRDS_ARE_GO) {
		printf("\033[01;36mthunderbirds are go.\033[00m\n");
	} else if (m->tag == DATA_RACE) {
		printf("\033[01;33mdr %d@0x%x (%x/%x/%d/%d/%d): %s\033[00m\n",
		       m->content.dr.tid, m->content.dr.eip,
		       m->content.dr.last_call, m->content.dr.most_recent_syscall,
		       m->content.dr.confirmed, m->content.dr.deterministic,
		       m->content.dr.free_re_malloc, m->content.dr.pretty_printed);
	} else if (m->tag == ESTIMATE) {
		printf("\033[01;32mprogress: %u/%u (%u/%u secs), ICB@%u\033[00m\n",
		       m->content.estimate.elapsed_branches,
		       (unsigned int)(m->content.estimate.elapsed_branches /
		                      m->content.estimate.proportion),
		       (unsigned int)(m->content.estimate.elapsed_usecs / 1000000),
		       (unsigned int)(m->content.estimate.total_usecs / 1000000),
		       m->content.estimate.icb_cur_bound);
	} else if (m->tag == FOUND_A_BUG) {
		printf("\033[01;31mFAB: %s ICB@%u\033[00m\n",
		       m->content.bug.trace_filename,
		       m->content.bug.icb_preemption_count);
	} else if (m->tag == SHOULD_CONTINUE) {
		printf("\033[00;37mshould continue? (yes)\033[00m\n");
	} else if (m->tag == ASSERT_FAILED) {
		printf("\033[01;35massert failed: %s\033[00m\n",
		       m->content.crash_report.assert_message);
	} else {
		printf("unknown message type!\n");
	}
}

void recv(struct input_message *m)
{
	if (use_pipes) {
		int ret = read(input_fd, m, sizeof(struct input_message));
		if (ret == 0) {
			/* pipe closed */
			m->tag = SHOULD_CONTINUE_REPLY;
			m->value = true;
		} else if (ret != sizeof(struct input_message)) {
			assert(false && "read failed");
		} else {
			assert(m->magic == message_magic && "wrong magic");
		}
	} else {
		m->tag = SHOULD_CONTINUE_REPLY;
		m->value = false;
	}
}

bool read_log(struct output_message *m, uint64_t *sleep_usecs)
{
	int ret = read(log_fd, m, sizeof(struct output_message));
	if (ret != sizeof(struct output_message)) {
		return false;
	}
	assert(m->magic == message_magic && "wrong message magic in log");

	ret = read(log_fd, sleep_usecs, sizeof(uint64_t));
	assert(ret == sizeof(uint64_t) && "log truncated?");

	return true;
}

void message_assert_fail(const char *message,  const char *file,
			 unsigned int line, const char *function)
{
	struct output_message m;
	m.tag = ASSERT_FAILED;
	snprintf(m.content.crash_report.assert_message, MESSAGE_BUF_SIZE,
		 "%s:%u: %s(): %s", file, line, function, message);
	send(&m);
	exit(13);
}

/******************************************************************************
 * main
 ******************************************************************************/

const char *sample_trace[] = {
	"<html><head><title>",
	"landslide preemption trace output",
	"</title><style>",
	"table,th,td { border:1px solid black; }",
	"</style></head><body>",
	"<span style=\"color: #cc0000;\"><h2>A bug was found!</h2>",
	"</span>Current stack (TID 4):<br />",
	"0x001026d3 in <span style=\"color: #008888;\"><b>JeEZAsscFUBi</b></span> <span style=\"color: #666666;\"><small>&lt;unknown&gt;</small></span>",
	"<br />0x0100241d in <span style=\"color: #008888;\"><b>_doprnt</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/libstdio/doprnt.c:211)</small></span>",
	"<br />0x010022b0 in <span style=\"color: #008888;\"><b>vsnprintf</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/libstdio/sprintf.c:74)</small></span>",
	"<br />0x01001e95 in <span style=\"color: #008888;\"><b>ferror</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/user/libsyscall/util.c:27)</small></span>",
	"<br />0x01003c54 in <span style=\"color: #880088;\">&lt;unknown in userspace&gt;</span>",
	"<span style=\"color: #cc0000;\"><h3>Kernel page faulted! Faulting eip: 0x102917; address: 0xfffe6fa4</h3>",
	"</span>Distinct interleavings tested: 2<br />",
	"Estimated state space size: 8.000000<br />",
	"Estimated state space coverage: 25.000000%<br />",
	"<br />",
	"<table><tr>",
	"<td><div style=\"font-size:large;text-align:center\">TID 3 (shell)</div></td>",
	"<td><div style=\"font-size:large;text-align:center\">TID 4</div></td>",
	"<td><div style=\"font-size:large;text-align:center\">TID 5</div></td>",
	"</tr>",
	"<tr><td>0x00105c41 in <span style=\"color: #008888;\"><b>[context switch]</b></span> <span style=\"color: #666666;\"><small>(kernel__pathos.o:0)</small></span>",
	"<br />0x01001cf5 in <span style=\"color: #880088;\">&lt;unknown in userspace&gt;</span>",
	"</td><td></td><td></td></tr>",
	"<tr><td></td><td>0x00105c41 in <span style=\"color: #008888;\"><b>[context switch]</b></span> <span style=\"color: #666666;\"><small>(kernel__pathos.o:0)</small></span>",
	"<br />0x01001207 in <span style=\"color: #008888;\"><b>thr_create_helper</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/progs/thr_exit_join.c:31)</small></span>",
	"<br />0x01000684 in <span style=\"color: #008888;\"><b>thr_create</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/user/libthread/thr.c:449)</small></span>",
	"<br />0x01000059 in <span style=\"color: #008888;\"><b>main</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/progs/thr_exit_join.c:28)</small></span>",
	"<br />0x01000092 in <span style=\"color: #008888;\"><b>_main</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/crt0.c:18)</small></span>",
	"<br />0xdeadd00d in <span style=\"color: #880088;\">&lt;unknown in userspace&gt;</span>",
	"</td><td></td></tr>",
	"<br />0x01001e66 in <span style=\"color: #008888;\"><b>yield</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/progs/thr_exit_join.c:31)</small></span>",
	"<br />0x01000d6c in <span style=\"color: #008888;\"><b>mutex_lock</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/user/libthread/mutex.c:81)</small></span>",
	"<br />0x01000735 in <span style=\"color: #008888;\"><b>thr_join</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/user/libthread/thr.c:506)</small></span>",
	"<br />0x01000018 in <span style=\"color: #008888;\"><b>waiter</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/progs/thr_exit_join.c:13)</small></span>",
	"<br />0x01001247 in <span style=\"color: #008888;\"><b>thr_exit_wrapper</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/progs/thr_exit_join.c:31)</small></span>",
	"<br />0x00000000 in <span style=\"color: #880088;\">&lt;zero-town&gt;</span>",
	"<br />0x01000684 in <span style=\"color: #008888;\"><b>thr_create</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/user/libthread/thr.c:449)</small></span>",
	"<br />0x01000059 in <span style=\"color: #008888;\"><b>main</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/progs/thr_exit_join.c:28)</small></span>",
	"<br />0x01000092 in <span style=\"color: #008888;\"><b>_main</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/crt0.c:18)</small></span>",
	"<br />0xdeadd00d in <span style=\"color: #880088;\">&lt;unknown in userspace&gt;</span>",
	"</td></tr>",
	"<br />0x01001e66 in <span style=\"color: #008888;\"><b>yield</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/progs/thr_exit_join.c:31)</small></span>",
	"<br />0x01000d6c in <span style=\"color: #008888;\"><b>mutex_lock</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/user/libthread/mutex.c:81)</small></span>",
	"<br />0x01000807 in <span style=\"color: #008888;\"><b>thr_exit</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/user/libthread/thr.c:558)</small></span>",
	"<br />0x01000065 in <span style=\"color: #008888;\"><b>main</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/progs/thr_exit_join.c:31)</small></span>",
	"<br />0x01000092 in <span style=\"color: #008888;\"><b>_main</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/crt0.c:18)</small></span>",
	"<br />0xdeadd00d in <span style=\"color: #880088;\">&lt;unknown in userspace&gt;</span>",
	"</td><td></td></tr>",
	"<tr><td></td><td></td><td>0x00105c41 in <span style=\"color: #008888;\"><b>[context switch]</b></span> <span style=\"color: #666666;\"><small>(kernel__pathos.o:0)</small></span>",
	"<br />0x01001e0f in <span style=\"color: #008888;\"><b>deschedule</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/progs/thr_exit_join.c:31)</small></span>",
	"<br />0x010010c3 in <span style=\"color: #008888;\"><b>cond_wait</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/user/libthread/cond.c:195)</small></span>",
	"<br />0x010007b4 in <span style=\"color: #008888;\"><b>thr_join</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/user/libthread/thr.c:527)</small></span>",
	"<br />0x01000018 in <span style=\"color: #008888;\"><b>waiter</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/progs/thr_exit_join.c:13)</small></span>",
	"<br />0x01001247 in <span style=\"color: #008888;\"><b>thr_exit_wrapper</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/progs/thr_exit_join.c:31)</small></span>",
	"<br />0x00000000 in <span style=\"color: #880088;\">&lt;zero-town&gt;</span>",
	"<br />0xffffffff in <span style=\"color: #880088;\">&lt;unknown in userspace&gt;</span>",
	"<br />0x453b0840 in <span style=\"color: #880088;\">&lt;unknown in userspace&gt;</span>",
	"</td></tr>",
	"<tr><td></td><td>0x01000dce in <span style=\"color: #008888;\"><b>mutex_unlock</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/user/libthread/mutex.c:106)</small></span>",
	"</td><td></td></tr>",
	"<tr><td></td><td></td><td>0x00105c41 in <span style=\"color: #008888;\"><b>[context switch]</b></span> <span style=\"color: #666666;\"><small>(kernel__pathos.o:0)</small></span>",
	"<br />0x01001e62 in <span style=\"color: #008888;\"><b>vanish</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/progs/thr_exit_join.c:31)</small></span>",
	"<br />0x0100123c in <span style=\"color: #008888;\"><b>thr_exit_helper</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/progs/thr_exit_join.c:31)</small></span>",
	"</td></tr>",
	"<tr><td></td><td>0x001026d3 in <span style=\"color: #008888;\"><b>JeEZAsscFUBi</b></span> <span style=\"color: #666666;\"><small>&lt;unknown&gt;</small></span>",
	"<br />0x0100241d in <span style=\"color: #008888;\"><b>_doprnt</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/libstdio/doprnt.c:211)</small></span>",
	"<br />0x010022b0 in <span style=\"color: #008888;\"><b>vsnprintf</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/410user/libstdio/sprintf.c:74)</small></span>",
	"<br />0x01001e95 in <span style=\"color: #008888;\"><b>ferror</b></span> <span style=\"color: #666666;\"><small>(p2-basecode/user/libsyscall/util.c:27)</small></span>",
	"<br />0x01003c54 in <span style=\"color: #880088;\">&lt;unknown in userspace&gt;</span>",
	"</td><td></td></tr>",
	"</table>",
	"</body>",
	"</html>",
};

// usage: ./landslide-aec LOGFILE MAGIC
// (for manual use / debugging)
// usage: ./landslide-aec LOGFILE MAGIC INPUT_PIPE OUTPUT_PIPE
// (for being called by quicksand glue scripts)
// in either case, MAGIC is probably 0x15410de0u

int main(int argc, char **argv)
{
	if (argc != 3 && argc != 5) {
		FAIL_PRE_INIT("wrong number of args");
	}

	log_fd = open(argv[1], O_RDONLY);
	if (log_fd <= 0) FAIL_PRE_INIT("failed open log file");

	message_magic = (unsigned int)strtol(argv[2], NULL, 0);

	if ((use_pipes = (argc == 5))) {
		output_fd = open(argv[4], O_WRONLY);
		if (output_fd <= 0) FAIL_PRE_INIT("failed open output pipe");
	}

	struct output_message om;
	struct input_message im;
	uint64_t sleep_usecs;

	bool thunderbirds = read_log(&om, &sleep_usecs);
	assert(thunderbirds);
	assert(om.tag == THUNDERBIRDS_ARE_GO);
	send(&om);

	if (use_pipes) {
		input_fd = open(argv[3], O_RDONLY);
		if (input_fd <= 0) FAIL_PRE_INIT("failed open input pipe");
	}

	while (read_log(&om, &sleep_usecs)) {
		usleep((unsigned int)sleep_usecs);
		send(&om);
		if (om.tag == ESTIMATE) {
			recv(&im);
			if (im.tag == SUSPEND_TIME && im.value) {
				recv(&im);
				assert(im.tag == RESUME_TIME ||
				       im.tag == SHOULD_CONTINUE_REPLY);
			}
		} else if (om.tag == SHOULD_CONTINUE) {
			recv(&im);
			assert(im.tag == SHOULD_CONTINUE_REPLY);
			if (im.value) {
				break;
			}
		} else if (om.tag == FOUND_A_BUG) {
			int trace_fd = open(om.content.bug.trace_filename,
					    O_CREAT | O_WRONLY);
			if (trace_fd > 0) {
				for (unsigned int i = 0; i < sizeof(sample_trace) /
				                    sizeof(sample_trace[0]); i++) {
					write(trace_fd, sample_trace[i],
					      strlen(sample_trace[i]));
					write(trace_fd, "\n", 1);
				}
				close(trace_fd);
			}
		}
	}

	if (use_pipes) {
		close(input_fd);
		close(output_fd);
	}
	close(log_fd);
	return 0;
}
