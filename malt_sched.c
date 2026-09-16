/* This is the only file you will be editing.
 * - malt_sched.c (Malt Scheduler Library Code)
 * - Copyright of Starter Code: Prof. Kevin Andrea, George Mason University. All Rights Reserved
 * - Copyright of Student Code: You!  
 * - Restrictions on Student Code: Do not post your code on any public site (eg. Github).
 * -- Feel free to post your code on a PRIVATE Github and give interviewers access to it.
 * -- You are liable for the protection of your code from others.
 * - Date: Aug 2026
 */

/* CS367 Project 1, Fall Semester, 2026
 * Fill in your Name, GNumber, and Section Numbers in the following comment fields
 * Name:  Farhan Sikandar
 * GNumber:  G#01446524
 * Section Number:    CS367-001             (Replace the __ with your lecture section number)
 * Recitation Number: CS367-304             (Replace the __ with your recitation section number)
 */

/* malt CPU Scheduling Library */
 
/* Standard Library Includes */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
/* Unix System Includes */
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <pthread.h>
#include <sched.h>

/* DO NOT CHANGE THE FOLLOWING INCLUDES - Local Includes 
 * If you change these, it will not build on Zeus with the Makefile
 * If you change these, it will not run in the grader
 */
  #include "malt_sched.h"
  #include "strawhat_scheduler.h"
  #include "strawhat_support.h"
  #include "strawhat_process.h"
/* DO NOT CHANGE ABOVE INCLUDES - Local Includes */


/* Edit the file below this line....
 * Feel free to create any definitions or constants you like!
 * Feel free to create any helper functions you like! 
 */

#define STATE_READY       (1 << BIT_READY)      /* 0x1000 */
#define STATE_RUNNING     (1 << BIT_RUNNING)    /* 0x0800 */
#define STATE_SUSPENDED   (1 << BIT_SUSPENDED)  /* 0x0400 */
#define STATE_TERMINATED  (1 << BIT_TERMINATED) /* 0x0200 */
#define STATE_CRITICAL    (1 << BIT_CRITICAL)   /* 0x0100 */

/* Mask covering just the State bits */
#define STATE_BITS_MASK (STATE_READY | STATE_RUNNING | STATE_SUSPENDED | STATE_TERMINATED)

/* Mask covering the lower 8 bits used for the Exit Code */
#define EXIT_CODE_MASK ((1 << BITS_EXIT_CODE) - 1) /* 0x00FF */


/* Clears the 4 State bits (Ready/Running/Suspended/Terminated) and sets exactly
 * one of them, leaving the Critical bit and the Exit Code bits untouched. */
static void set_state(Malt_process_s *process, unsigned short new_state_bit) {
  process->state = (process->state & ~STATE_BITS_MASK) | new_state_bit;
}

/* Appends a single already-detached Node to the end */
static void queue_append(Malt_queue_s *queue, Malt_process_s *process) {
  process->next = NULL;
  if (queue->head == NULL) {
    queue->head = process;
    queue->tail = process;
  } else {
    queue->tail->next = process;
    queue->tail = process;
  }
  queue->count++;
}

/* Removes and returns the Node with the given pid from the Queue's linked list */
static Malt_process_s *queue_remove(Malt_queue_s *queue, pid_t pid) {
  Malt_process_s *prev = NULL;
  Malt_process_s *curr = queue->head;

  while (curr != NULL) {
    if (pid == 0 || curr->pid == pid) {
      if (prev == NULL) {
        queue->head = curr->next;
      } else {
        prev->next = curr->next;
      }
      if (curr == queue->tail) {
        queue->tail = prev;
      }
      curr->next = NULL;
      queue->count--;
      return curr;
    }
    prev = curr;
    curr = curr->next;
  }
  return NULL;
}

/* Frees a single Process Node */
static void free_process(Malt_process_s *process) {
  if (process == NULL) {
    return;
  }
  if (process->cmd != NULL) {
    free(process->cmd);
  }
  free(process);
}

/* Frees every Node currently in the given Queue's list */
static void free_queue_nodes(Malt_queue_s *queue) {
  Malt_process_s *curr = queue->head;
  Malt_process_s *next;

  while (curr != NULL) {
    next = curr->next;
    free_process(curr);
    curr = next;
  }
  queue->head = NULL;
  queue->tail = NULL;
  queue->count = 0;
}

/*** Malt Library API Functions to Complete ***/

/* Initializes the Malt_schedule_s Struct and all of the Malt_queue_s Structs
 * Follow the project documentation for this function.
 * Returns a pointer to the new Malt_schedule_s or NULL on any error.
 * - Hint: What does malloc return on an error?
 */
Malt_schedule_s *malt_initialize() {
  Malt_schedule_s *schedule = malloc(sizeof(Malt_schedule_s));
  if (schedule == NULL) {
    return NULL;
  }

  schedule->ready_queue = malloc(sizeof(Malt_queue_s));
  schedule->suspended_queue = malloc(sizeof(Malt_queue_s));
  schedule->terminated_queue = malloc(sizeof(Malt_queue_s));

  if (schedule->ready_queue == NULL ||
      schedule->suspended_queue == NULL ||
      schedule->terminated_queue == NULL) {
    
    free(schedule->ready_queue);
    free(schedule->suspended_queue);
    free(schedule->terminated_queue);
    free(schedule);
    return NULL;
  }

  Malt_queue_s *queues[3] = { schedule->ready_queue,
                              schedule->suspended_queue,
                              schedule->terminated_queue };
  for (int i = 0; i < 3; i++) {
    queues[i]->count = 0;
    queues[i]->head = NULL;
    queues[i]->tail = NULL;
  }

  return schedule;
}

/* Allocates and Initializes a new Malt_process_s with the given information.
 * - Malloc and copy the command string, don't just assign it!
 * Follow the project documentation for this function.
 * - You may assume all members within data are Legal and Correct for this Function Only
 * Returns a pointer to the new Malt_process_s on success or a NULL on any error.
 */
Malt_process_s *malt_create(Malt_create_data_s *data) {
  if (data == NULL) {
    return NULL;
  }

  Malt_process_s *process = malloc(sizeof(Malt_process_s));
  if (process == NULL) {
    return NULL;
  }

  size_t len = strlen(data->original_cmd) + 1;
  process->cmd = malloc(len);
  if (process->cmd == NULL) {
    free(process);
    return NULL;
  }
  strncpy(process->cmd, data->original_cmd, len);

  process->pid = data->pid;
  process->priority = data->priority_level;
  process->age = 0;
  process->next = NULL;

  
  process->state = 0;
  process->state |= STATE_READY;
  if (data->is_critical) {
    process->state |= STATE_CRITICAL;
  }

  return process;
}

/* Add a process into the end of Ready Queue (singly linked list).
 * Follow the project documentation for this function.
 * - Do not create a new process to insert, insert the SAME process passed in.
 * Returns a 0 on success or a -1 on any error.
 */
int malt_add(Malt_schedule_s *schedule, Malt_process_s *process) {
  if (schedule == NULL || schedule->ready_queue == NULL || process == NULL) {
    return -1;
  }

  set_state(process, STATE_READY);
  queue_append(schedule->ready_queue, process);

  return 0;
}

/* Returns the number of items in a given Malt Queue (singly linked list).
 * Follow the project documentation for this function.
 * Returns the number of processes in the list or -1 on any errors.
 */
int malt_count(Malt_queue_s *queue) {
  if (queue == NULL) {
    return -1;
  }
  return queue->count;
}

/* Selects the best process to run from the Ready Queue (singly linked list).
 * Follow the project documentation for this function.
 * Returns a pointer to the process selected or NULL if none available or on any errors.
 * - Do not create a new process to return, return a pointer to the SAME process selected.
 * - Return NULL if the ready queue was empty OR if there were any errors.
 */
Malt_process_s *malt_select(Malt_schedule_s *schedule) {
  if (schedule == NULL || schedule->ready_queue == NULL) {
    return NULL;
  }

  Malt_queue_s *ready = schedule->ready_queue;
  if (ready->head == NULL) {
    return NULL;
  }

  Malt_process_s *best = NULL;
  Malt_process_s *curr;

  /* First Critical process found */
  for (curr = ready->head; curr != NULL; curr = curr->next) {
    if (curr->state & STATE_CRITICAL) {
      best = curr;
      break;
    }
  }

  /* Otherwise, first Starving process found */
  if (best == NULL) {
    for (curr = ready->head; curr != NULL; curr = curr->next) {
      if (curr->age >= STARVING_AGE) {
        best = curr;
        break;
      }
    }
  }

  /* Otherwise, the highest priority; ties go to whichever was found first */
  if (best == NULL) {
    for (curr = ready->head; curr != NULL; curr = curr->next) {
      if (best == NULL || curr->priority < best->priority) {
        best = curr;
      }
    }
  }

  if (best == NULL) {
    return NULL;
  }

  /* Remove the chosen process from the Ready Queue */
  Malt_process_s *removed = queue_remove(ready, best->pid);
  if (removed == NULL) {
    return NULL;
  }

  removed->age = 0;
  set_state(removed, STATE_RUNNING);
  removed->next = NULL;

  /* Age everyone still waiting in the Ready Queue */
  for (curr = ready->head; curr != NULL; curr = curr->next) {
    curr->age++;
  }

  return removed;
}

/* Moves the process with matching pid from Ready to Suspended Queue.
 * Follow the specification for this function.
 * Returns a 0 on success or a -1 on any error (such as process not found).
 */
int malt_suspend(Malt_schedule_s *schedule, pid_t pid) {
  if (schedule == NULL || schedule->ready_queue == NULL || schedule->suspended_queue == NULL) {
    return -1;
  }

  Malt_process_s *process = queue_remove(schedule->ready_queue, pid);
  if (process == NULL) {
    return -1;
  }

  set_state(process, STATE_SUSPENDED);
  queue_append(schedule->suspended_queue, process);

  return 0;
}

/* Moves the process with matching pid from Suspended to Ready Queue.
 * Follow the specification for this function.
 * Returns a 0 on success or a -1 on any error (such as process not found).
 */
int malt_resume(Malt_schedule_s *schedule, pid_t pid) {
  if (schedule == NULL || schedule->suspended_queue == NULL || schedule->ready_queue == NULL) {
    return -1;
  }

  Malt_process_s *process = queue_remove(schedule->suspended_queue, pid);
  if (process == NULL) {
    return -1;
  }

  set_state(process, STATE_READY);
  queue_append(schedule->ready_queue, process);

  return 0;
}

/* This is called when a process exits normally that was just Running.
 * Puts the given node into the Terminated Queue and sets the Exit Code 
 * - Do not create a new process to insert, insert the SAME process passed in.
 * Follow the project documentation for this function.
 * Returns a 0 on success or a -1 on any error.
 */
int malt_exited(Malt_schedule_s *schedule, Malt_process_s *process, int exit_code) {
  if (schedule == NULL || schedule->terminated_queue == NULL || process == NULL) {
    return -1;
  }

  set_state(process, STATE_TERMINATED);
  process->state = (process->state & ~EXIT_CODE_MASK) | (exit_code & EXIT_CODE_MASK);

  queue_append(schedule->terminated_queue, process);

  return 0;
}

/* This is called when the OS terminates a process early. 
 * - The matching process will either be in your Ready Queue or Suspended Queue.
 * - The difference with malt_exited is that this process is in one of your Queues already.
 * Removes the process with matching pid from either of these Queues and adds the Exit Code to it.
 * - You have to check both since it could be in either queue, if it exists at all.
 * Follow the project documentation for this function.
 * Returns a 0 on success or a -1 on any error.
 */
int malt_killed(Malt_schedule_s *schedule, pid_t pid, int exit_code) {
  if (schedule == NULL || schedule->ready_queue == NULL ||
      schedule->suspended_queue == NULL || schedule->terminated_queue == NULL) {
    return -1;
  }

  Malt_process_s *process = queue_remove(schedule->ready_queue, pid);
  if (process == NULL) {
    process = queue_remove(schedule->suspended_queue, pid);
  }
  if (process == NULL) {
    return -1;
  }

  set_state(process, STATE_TERMINATED);
  process->state = (process->state & ~EXIT_CODE_MASK) | (exit_code & EXIT_CODE_MASK);

  queue_append(schedule->terminated_queue, process);

  return 0;
}

/* This is called when StrawHat reaps a Terminated (Defunct) Process.  (reap command).
 * Removes and frees the process with matching pid from the Termainated Queue.
 * Follow the specification for this function.
 * Returns the exit_code on success or a -1 on any error (such as process not found).
 */
int malt_reap(Malt_schedule_s *schedule, pid_t pid) {
  if (schedule == NULL || schedule->terminated_queue == NULL) {
    return -1;
  }

  Malt_process_s *process = queue_remove(schedule->terminated_queue, pid);
  if (process == NULL) {
    return -1;
  }

  int exit_code = process->state & EXIT_CODE_MASK;
  free_process(process);

  return exit_code;
}

/* Returns the exit code if the process is terminated.
 * (All Linux exit codes are between 0 and 255)
 * Follow the project documentation for this function.
 * If the process is not terminated, return -1.
 */
int malt_get_ec(Malt_process_s *process) {
  if (process == NULL) {
    return -1;
  }
  if (!(process->state & STATE_TERMINATED)) {
    return -1;
  }
  return process->state & EXIT_CODE_MASK;
}

/* Frees all allocated memory in the Malt_schedule_s, all of the Queues, and all of their Nodes.
 * Follow the project documentation for this function.
 * Returns void.
 */
void malt_deallocate(Malt_schedule_s *schedule) {
  if (schedule == NULL) {
    return;
  }

  if (schedule->ready_queue != NULL) {
    free_queue_nodes(schedule->ready_queue);
    free(schedule->ready_queue);
  }
  if (schedule->suspended_queue != NULL) {
    free_queue_nodes(schedule->suspended_queue);
    free(schedule->suspended_queue);
  }
  if (schedule->terminated_queue != NULL) {
    free_queue_nodes(schedule->terminated_queue);
    free(schedule->terminated_queue);
  }

  free(schedule);
}
