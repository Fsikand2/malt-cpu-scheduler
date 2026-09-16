# Malt CPU Scheduler

**Language:** C
**Focus:** Systems programming, linked lists, memory management, and scheduling algorithms

This project implements a priority-based CPU scheduling module in C for the instructor-provided StrawHat framework. It manages ready, suspended, and terminated process queues and selects processes for execution based on critical status, waiting age, and priority.

## Key Features

* Priority-based process selection, with critical processes selected first.
* Process aging to give long-waiting processes precedence over ordinary priority selection.
* Singly linked-list queues with head, tail, and process-count tracking.
* Process suspension, resumption, termination, and reaping.
* Bitwise operations for process state flags and exit-code storage.
* Dynamic memory allocation and cleanup for process nodes and command strings.

## Technical Highlights

* Manual memory management using `malloc` and `free`.
* Linked-list insertion, removal, and traversal.
* Bit masking to update state flags while preserving other fields.
* Checks for invalid pointers, empty queues, and allocation failures.

**Note:** This repository contains my implementation in `malt_sched.c`, developed from the course-provided starter file. The surrounding framework, headers, and build files are not included, so this file cannot compile or run independently..
