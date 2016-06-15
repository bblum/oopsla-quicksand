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
options on to your MC.

The static config options are:

- TEST_CASE (env var): Whatever program is passed to quicksand via "-p"
- VERBOSE (env var): 0 or 1 depending if "-v" is specified
- ICB (env var): 0 or 1 depending if "-I" is specified ("SSS-MC-ICB" in the paper)
- PREEMPT_EVERYWHERE (env var): 0 or 1 depending if "-0" is specified (called
  "SSS-MC-Shared-Mem" in the paper)
- id_magic (function): A magic number which quicksand will send to identify its
  version in the messaging protocol, used for assertions
- DR_PPS_RESPECT_WITHIN_FUNCTIONS (env var): 0, 1, or absent. Used with
  PREEMPT_EVERYWHERE to indicate [...] TODO
