/*
 *  Copyright(c) 2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>


int err;

static void __check(const char *filename, int line, int x, int expect)
{
    if (x != expect) {
        printf("ERROR %s:%d - %d != %d\n",
               filename, line, x, expect);
        err++;
    }
}

#define check(x, expect) __check(__FILE__, __LINE__, (x), (expect))

static int satub(int src, int *p, int *ovf_result)
{
    int result;
    int usr;

    /*
     * This instruction can set bit 0 (OVF/overflow) in usr
     * Clear the bit first, then return that bit to the caller
     *
     * We also store the src into *p in the same packet, so we
     * can ensure the overflow doesn't get set when an exception
     * is generated.
     */
    asm volatile("r2 = usr\n\t"
                 "r2 = clrbit(r2, #0)\n\t"        /* clear overflow bit */
                 "usr = r2\n\t"
                 "{\n\t"
                 "    %0 = satub(%2)\n\t"
                 "    memw(%3) = %2\n\t"
                 "}\n\t"
                 "%1 = usr\n\t"
                 : "=r"(result), "=r"(usr)
                 : "r"(src), "r"(p)
                 : "r2", "usr", "memory");
  *ovf_result = (usr & 1);
  return result;
}

int read_usr_overflow(void)
{
    int result;
    asm volatile("%0 = usr\n\t" : "=r"(result));
    return result & 1;
}


jmp_buf jmp_env;
int usr_overflow;

static void sig_segv(int sig, siginfo_t *info, void *puc)
{
    usr_overflow = read_usr_overflow();
    longjmp(jmp_env, 1);
}

int main()
{
    struct sigaction act;
    int ovf;

    /* SIGSEGV test */
    act.sa_sigaction = sig_segv;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &act, NULL);
    if (setjmp(jmp_env) == 0) {
        satub(300, 0, &ovf);
    }

    act.sa_handler = SIG_DFL;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    check(usr_overflow, 0);

    puts(err ? "FAIL" : "PASS");
    return err ? EXIT_FAILURE : EXIT_SUCCESS;
}
