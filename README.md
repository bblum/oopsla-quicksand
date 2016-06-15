==== Foreword ====

Our artifact is Quicksand, the tool presented in our paper for managing
multiple model checker instances with different preemption-point
configurations.

Regrettably, our model checker Landslide requires the proprietary simulator
Simics, whose license we are not able to share with the AEC. Instead, we have
created a "dummy" version of Landslide that replays recorded logs from our
experiments, and provided a selection of logs from a variety of the tests in
our evaluation. Quicksand itself is unchanged, as the dummy Landslide
effectively implements its message-passing interface in the same black-box way
as the real Landslide.

We hope that, despite being limited by license restrictions on the model
checker itself, this artifact convincingly demonstrates Quicksand and its
ability to be adapted with other available checkers.

==== Getting Started Guide ====

---- Setting up ----

Our artifact is provided in a VM image that we made using VirtualBox version
5.0.20.106931. To set up, add the image in VirtualBox as a 64-bit Ubuntu
machine, log in as "aec" with password "aec", and cd to /home/aec/quicksand/.
Please configure the VM to use several processors (ideally at least 4) to take
full advantage of Quicksand's parallelism.

Within this directory:
- 'qs', containing Quicksand's source,
- 'ls', containing the dummy Landslide and our pre-recorded message logs,
- 'quicksand', a brief wrapper script for qs/quicksand,
- 'sample', a directory with several example commands and expected outputs

qs/ and ls/ contain prebuilt binaries for Quicksand and Landslide, but you can
refresh them with a simple make clean && make.

---- Basic testing ----

The basic command we recommend to run in various incarnations is:

    ./quicksand -p PROGRAM_NAME -c NUM_CPUS -t TIME_LIMIT CONFIG_FLAGS...

PROGRAM_NAME is one of broadcast_test, thr_exit_join, paradise_lost
NUM_CPUS self-explanatory
TIME_LIMIT number of seconds, can also be e.g. 5m, 2h, 1d for minutes/hours/days
CONFIG_FLAGS include -v for verbose output, and -C and -I for control
experiments (see below)
("-h" of course prints help text.)

We provided logs for three of our test programs in three configurations each.

The programs are:
- broadcast_test, which demonstrates a full verification (all data race PPs can
  be tested completely within an hour)
- thr_exit_join (called "join_test" in the paper), which demonstrates finding a
  bug (<= 1 minute)
- paradise_lost ("sem_test" in the paper), which demonstrates tests with large
  ETAs that must be suspended and/or time out.

The configurations are:
- Quicksand (the default, no CONFIG_FLAGS)
- Control test (1 state space, no data-race PPs) with DPOR (use "-C" flag)
- Control test (1 state space, no data-race PPs) with ICB (use "-C -I" flags)

For example,

    ./quicksand -p broadcast_test -t 60m -c 4

will run a Quicksand full-verification experiment with 1 wall-clock hour times 4 parallel jobs.

---- Further notes ----

There are also some secret options for more ambitious users such as -e and -E,
for adjusting the ETA heuristics which decide when Quicksand will suspend a job
with a high ETA compared to the TIME_LIMIT. For example "-e 4" will make
Quicksand optimistically run any big job whose ETA is less than 4 times the
remaining time in the limit (default 2x). "-E" will adjust the minimum number
of interleavings each job will always be allowed to test before its ETA is
considered "stable" (default 32).

You can potentially use these flags to make Quicksand suspend jobs that it
wouldn't otherwise suspend, which may lead to some emergent behaviour in how
jobs are scheduled (i.e., "pushing the limits of the artifact").

Since we are presenting a dummy log-replaying Landslide instead of running real
programs, you can also adjust the speed at which we replay the logs. By default
this Landslide usleep()s for a recorded amount of time between each message, to
simulate the time to execute each interleaving. You can hack this behaviour by
finding usleep() in ls/landslide-aec.c and deleting it, or dividing by 10, and
so on (and "make clean && make" obviously).

Quicksand may leave behind log and config files if you ctrl-C it. You can
safely delete these with ./cleanup-temp-files.sh.

==== Step by Step Instructions ====

The getting started guide roughly covers the extent of what can be done with
the pre-recorded logs we supplied for the AEC. This section will serve as a
guide to Quicksand's interface with the model checker (MC), to aid users who
wish to port their own model checker for use with Quicksand.

---- Config files ----

Quicksand executes each MC with two configuration files as command-line
arguments. The first of these is the "static" config: options that will not
change across jobs within a single run of Quicksand, such as which test program
to run, whether to run an ICB control experiment, etc. The second is the
"dynamic" config, containing options that are unique to each job, such as which
preemption points to use. (The reason for the two files is that Landslide needs
to do some automatic but expensive instrumentation whenever the test program
changes.)

These files are in a bash script format. Some options are specified as
environment variables, while others call bash functions, assuming that such
functions will be defined already. We recommend writing a wrapper script which
will define these functions, process the environment variables, and pass those
options on to your MC. Consult the ls/landslide script as an example.

The static config options are:

- TEST_CASE (env var): Whatever program is passed to quicksand via "-p"
- VERBOSE (env var): 0 or 1 depending if "-v" is specified
- ICB (env var): 0 or 1 depending if "-I" is specified ("SSS-MC-ICB" in the paper)
- PREEMPT_EVERYWHERE (env var): 0 or 1 depending if "-0" is specified (called
  "SSS-MC-Shared-Mem" in the paper)
- id_magic (function): A magic number which quicksand will send to identify its
  version in the messaging protocol, used for assertions

The dynamic config options are:

- TEST_CASE (env var): Same as before.
- without_function/within_function (functions): Indicates a list of functions
  which should be whitelisted or blacklisted when the MC checks where to
  insert preemption points. The MC should implement Preemption Sealing (see
  paper section 4.1) to interpret these commands. These are also used to
  indicate whether the static synchronization API PPs (mutex_lock,
  mutex_unlock, etc) should be considered for each job.
- data_race (function): Indicates a data-race preemption point. Arguments are:
  - the instruction pointer involved in the race,
  - the thread ID of the thread which executed it,
  - the address of the most recent "call" instruction, or 0 (described in
    messaging protocol below)
  - the interrupt number of the most recent system call, or 0 (described below)
- input_pipe/output_pipe (functions): Filenames of the FIFO files which should
  be used for message-passing.

Those who prefer to source-dive to figure out how these options are issued
should start their search in run_job() in qs/job.c.

---- Messaging interface ----

During execution, Quicksand and the MC communicate by sending messages over
two FIFO files, the input pipe (messages from QS to MC) and the output pipe
(from MC to QS). This corresponds to input_pipe/output_pipe in the config
files (although in the Quicksand source, these names are reversed). The format
of the messages is specified at the top of qs/messaging.c.

There are 6 types of output messages (see "enum tag"). Unless otherwise
specified below, the MC should just send messages and not wait for replies.
When the MC does need to wait for a reply, there are 3 types of reply (input)
messages (see the other "enum tag").

Both message types have a "magic" field, which should match the "id_magic"
value provided in the static config file, and a "tag" field which indicates
the message type (as well as which branch of the subsequent union to use).
It's just a sum type in C.

Output messages:
- THUNDERBIRDS_ARE_GO: Sent once during initialization when the MC is ready to
  start testing. (Note that because of the nature of FIFOs, to avoid deadlock,
  this message must be sent between opening the output pipe and opening the
  input pipe.)
- DATA_RACE: Sent whenever the MC detects a new data-race candidate. Fields:
  - content.dr.eip: The instruction pointer of the race
  - content.dr.tid: The TID which executed that eip
  - content.dr.last_call: The address of the last "call" instruction preceding
    the race. The MC may optionally specify this to filter the identification
    of data-race candidates if there are too many, or it may just send 0 as a
    wild-card.
  - content.dr.most_recent_syscall: The number of the most recent system call
    before the racing instruction, if in kernel-space. 0 if the race came from
    user-space. This may be used when kernel system calls write into shared
    user memory, such as read(). This option and last_call are not interpreted
    by Quicksand, used only for equality comparison and parroted back to other
    MC instances via the config files.
  - content.dr.confirmed: True if the MC has classified the race as
    "both-order" (see paper section 3.4, and paper citation [46]). Used for
    heuristic job priorities.
  - content.dr.deterministic: True if the MC observed the race on the first
    branch. Used for our "nondeterministic race" experiment (paper section
    6.4).
  - content.dr.free_re_malloc: True if it's a malloc-recycle candidate (paper
    section 5.2 / 6.4)
- ESTIMATE: Sent at the completion of each tested interleaving to communicate
  a state space estimate. Fields:
  - content.estimate.proportion: Between 0.0L and 1.0L. Indicates expected
    proportion of the tree already explored.
  - content.estimate.elapsed_branches: Number completed interleavings tested
  - content.estimate.total_usecs: Estimated total time the state space will
    take (counting elapsed time)
  - content.estimate.elapsed_usecs: self-explanatory
  - content.estimate.icb_cur_bound: Current ICB bound; ignored without "-I"
- FOUND_A_BUG: Sent when the MC observes a failure. Fields:
  - content.bug.trace_filename: Filename containing whatever debug output the
    MC provides.
  - content.bug.icb_preemption_count: How many preemptions, as defined by the
    ICB implementation, were needed to uncover this failure
- SHOULD_CONTINUE: Sent whenever the MC is able to exit due to a time-out of
  the CPU budget. Landslide sends this message after each ESTIMATE message.
- ASSERT_FAILED: Edit your assert infrastructure so that if your MC ever
  crashes, it lets Quicksand know. Otherwise Quicksand will hang on a read.
  - content.crash_report.assert_message: A string to relay to the user.

Input messages:
- SHOULD_CONTINUE_REPLY: Sent in reply to SHOULD_CONTINUE or ESTIMATE. If this
  message type, then "value" is set: false if should continue, true if should
  abort.  (I know, it's backwards, sorry...)
- SUSPEND_TIME: Sent in reply to ESTIMATE if Quicksand wants the job to
  suspend.
- RESUME_TIME: Sent after a SUSPEND_TIME if Quicksand resumes the job. Also
  sent to all deferred jobs when time is up, so they can exit cleanly (via
  SHOULD_CONTINUE).

Message protocol:
- Whenever the MC sends a ESTIMATE output message, it must read an input
  message in response. If that message is SUSPEND_TIME, it must then wait
  again (via read() on the input pipe) until a RESUME_TIME message comes.
- Whenever the MC sends a SHOULD_CONTINUE output message, it must read for a
  SHOULD_CONTINUE_REPLY.
- Otherwise, the MC shall not use read() on the input pipe.

Those who prefer to source-dive should use ls/landslide-aec.c as a starting
point for the messaging protocol.

---- Codebase ----

Finally, we provide a brief overview of the different components of Quicksand
for those with ideas to improve its algorithm on their own.

- pp.c implements a global registry of both statically known and dynamically
  discovered preemption points, and provides some utilities for subset and
  equality comparison between preemption point sets.
- work.c implements the multithreaded workqueue, and implements the Iterative
  Deepening algorithm for deciding which jobs to run next (should_work_block()
  and get_work(); Algorithm 1 in the paper). It also implements the scheduling
  logic for the periodic progress report thread (progress_report_thread()).
- job.c contains code for the dedicated threads which create and communicate
  with each MC instance. The config file interface as well as some process
  lifecycle management can be found in run_job().
- messaging.c implements the message-passing protocol (talk_to_child()), and
  contains the algorithm for adding new jobs based on data race reports
  (handle_data_race(); Algorithm 2 in the paper). It also has some logic
  related to Algorithm 1 (handle_estimate()).
- option.c defines quicksand's command-line options. main.c processes them and
  hands off control to the work queue threads.
- bug.c, io.c, signals.c and time.c provide various boring utility functions.
