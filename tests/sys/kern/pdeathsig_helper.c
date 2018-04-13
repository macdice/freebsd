#include <assert.h>
#include <signal.h>
#include <sys/procctl.h>

int main(int argc, char **argv)
{
        int signum;
        int rc;

	/*
	 * This program is executed by the pdeathsig test
	 * to check if the PROC_PDEATHSIG_SET setting was
	 * inherited.
	 */
        signum = 0xdeadbeef;
        rc = procctl(P_PID, 0, PROC_PDEATHSIG_GET, &signum);
        assert(rc == 0);
        assert(signum == SIGINFO);

        return 0;
}
