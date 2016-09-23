/* -*- C -*-
 *
 * Copyright 2016 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Corporation, the U.S.  Government
 * retains certain rights in this software.
 *
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * This software is available to you under the BSD license.
 *
 * This file is part of the Sandia OpenSHMEM software package. For license
 * information, see the LICENSE file in the top level directory of the
 * distribution.
 *
 */

#include <shmemx.h>
#include <shmem-thread.h>

_Thread_local int             thread_is_registered = 0;
_Thread_local shmemx_ctx_t    thread_ctx;
_Thread_local shmemx_domain_t thread_domain;

_Atomic int num_threads;
_Atomic int barrier_cnt = 0;
_Atomic int barrier_complete = 0;
