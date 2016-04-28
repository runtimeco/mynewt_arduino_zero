/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#include <os/os.h>
#include <bsp/bsp.h>
#include <console/console.h>
#include <shell/shell.h>
#include <log/log.h>
#include <config/config.h>
#include <newtmgr/newtmgr.h>
#include <arduino_test/arduino_test.h>
#include <assert.h>
#include <string.h>

#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

/* Init all tasks */
volatile int tasks_initialized;
int init_tasks(void);

#define SHELL_TASK_PRIO (3)
#define SHELL_MAX_INPUT_LEN     (256)
#define SHELL_TASK_STACK_SIZE (OS_STACK_ALIGN(384))
os_stack_t shell_stack[SHELL_TASK_STACK_SIZE];

#define NEWTMGR_TASK_PRIO (4)
#define NEWTMGR_TASK_STACK_SIZE (OS_STACK_ALIGN(512))
os_stack_t newtmgr_stack[NEWTMGR_TASK_STACK_SIZE];

/**
 * init_tasks
 *
 * Called by main.c after os_init(). This function performs initializations
 * that are required before tasks are running.
 *
 * @return int 0 success; error otherwise.
 */
int
init_tasks(void)
{
    tasks_initialized = 1;
    return 0;
}

/**
 * main
 *
 * The main function for the project. This function initializes the os, calls
 * init_tasks to initialize tasks (and possibly other objects), then starts the
 * OS. We should not return from os start.
 *
 * @return int NOTE: this function should never return!
 */
int
main(int argc, char **argv)
{
    int rc;

#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif
    
    os_init();

    shell_task_init(SHELL_TASK_PRIO, shell_stack, SHELL_TASK_STACK_SIZE,
                    SHELL_MAX_INPUT_LEN);

    (void) console_init(shell_console_rx_cb);

    nmgr_task_init(NEWTMGR_TASK_PRIO, newtmgr_stack, NEWTMGR_TASK_STACK_SIZE);

    rc = arduino_test_init();
    assert(rc == 0);

    rc = init_tasks();
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}

