/*
 * Copyright 2011 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Corporation, the U.S.  Government
 * retains certain rights in this software.
 *
 *  Copyright (c) 2018 Intel Corporation. All rights reserved.
 *  This software is available to you under the BSD license below:
 *
 *      Redistribution and use in source and binary forms, with or
 *      without modification, are permitted provided that the following
 *      conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <shmem.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    int me, npes, status;
    pid_t pid;

    shmem_init();

    npes = shmem_n_pes();
    me = shmem_my_pe();

    pid = fork();

    if (pid == -1) {
        printf("%d: Fork failed\n", me);
        shmem_global_exit(1);
    }
    else if (pid == 0) {
        int err;
        printf("Hello World from %d of %d (child)\n", me, npes);
        err = execvp("true", NULL);
        if (err) {
            perror("execvp failed");
            abort();
        }
    }

    printf("Hello World from %d of %d (parent)\n", me, npes);
    waitpid(pid, &status, 0);

    if (status) {
        printf("%d: Child process exited with nonzero status (%d)\n", me, status);
        shmem_global_exit(2);
    }

    shmem_finalize();

    return 0;
}
