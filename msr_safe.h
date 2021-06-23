// Copyright 2011-2020 Lawrence Livermore National Security, LLC and other
// msr-safe Project Developers. See the top-level COPYRIGHT file for
// details.
//
// SPDX-License-Identifier: GPL-2.0-only

/*
 * This file defines the ioctl interface for submitting a batch of MSR
 * requests.  This file should be distributed to a place where kernel
 * module interface header files are kept for a distribution running this
 * module so that user-space applications and libraries that wish to use
 * the batch interface may #include this file to get the interface.
 */

#ifndef MSR_SAFE_HEADER_INCLUDE
#define MSR_SAFE_HEADER_INCLUDE

#include <linux/ioctl.h>
#include <linux/types.h>

struct msr_batch_op
{
    __u16 cpu;     // In: CPU to execute {rd/wr}msr instruction
    __u16 msrcmd;  // In: 0=wrmsr, 0x1=rdmsr.  See below for flags.
    __s32 err;     // Out: set if error occurred with this operation
    __u32 msr;     // In: MSR Address to perform operation
    __u64 msrdata; // In/Out: Input/Result to/from operation.  
    		   //   Name retained for backwards compatibility.
    __u64 wmask;   // Out: Write mask applied to wrmsr
    __u64 aperf0;  // Out: APERF at the beginning of the command.
    __u64 mperf0;  // Out: MPERF at the beginning of the command.
    __u64 aperf1;  // Out: APERF at the end of the command.
    __u64 mperf1;  // Out: MPERF at the end of the command.
    __u64 msrdata1;// Out: Value at the end of the command.
};

/* Flags for msrcmd:
 *
 * 0x0001	0=wrmsr, 1=rdmsr
 * 0x0002	If rdmsr, store initial read in msrdata0, poll until 
 * 		  the values changes, then store the new value in
 * 		  msrdata1.
 * 0x0004	If rdmsr, store APERF and MPERF values in aperf0
 *                and mperf0 before reading the target msr.
 * 0x0008       If rdmsr, store APERF and MPERF values in aperf1
 *                and mperf1 after reading the target msr.
 *
 * 0x1 | 0x2 | 0x4 | 0x8 = 0xf
 *  b0001
 *| b0010
 *| b0100
 *| b1000
 *-------
 *  b1111	
 *
 *  b????	b????
 * &b0001	b0010
 * ======	=====
 *  b000?	b00?0 
 *
 *
 */

struct msr_batch_array
{
    __u32 numops;             // In: # of operations in operations array
    struct msr_batch_op *ops; // In: Array[numops] of operations
};

#define X86_IOC_MSR_BATCH   _IOWR('c', 0xA2, struct msr_batch_array)

#endif
