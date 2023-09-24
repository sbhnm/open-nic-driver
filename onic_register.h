/*
 * Copyright (c) 2020 Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */
#ifndef __ONIC_REGISTER_H__
#define __ONIC_REGISTER_H__

#include "onic_hardware.h"

static inline u32 onic_read_reg(struct onic_hardware *hw, u32 offset)
{
	return ioread32(hw->addr + offset);
}

static inline void onic_write_reg(struct onic_hardware *hw, u32 offset, u32 val)
{
	iowrite32(val, hw->addr + offset);
}

#define SHELL_START					0x0
#define SHELL_END					0x10000
#define SHELL_MAXLEN					(SHELL_END - SHELL_START)

/***** system config registers *****/
#define SYSCFG_OFFSET					0x0

#define SYSCFG_OFFSET_BUILD_STATUS			(SYSCFG_OFFSET + 0x0)
#define SYSCFG_OFFSET_SYSTEM_RESET			(SYSCFG_OFFSET + 0x4)
#define SYSCFG_OFFSET_SYSTEM_STATUS			(SYSCFG_OFFSET + 0x8)
#define SYSCFG_OFFSET_SHELL_RESET			(SYSCFG_OFFSET + 0xC)
#define SYSCFG_OFFSET_SHELL_STATUS			(SYSCFG_OFFSET + 0x10)
#define SYSCFG_OFFSET_USER_RESET			(SYSCFG_OFFSET + 0x14)
#define SYSCFG_OFFSET_USER_STATUS			(SYSCFG_OFFSET + 0x18)

/***** QDMA subsystem registers *****/
#define QDMA_SUBSYSTEM_OFFSET				0x1000
#define QDMA_FUNC_OFFSET(i)				(QDMA_SUBSYSTEM_OFFSET + (0x1000 * i))
#define QDMA_SUBSYS_OFFSET				(QDMA_SUBSYSTEM_OFFSET + 0x4000)

#define QDMA_FUNC_OFFSET_QCONF(i)			((QDMA_FUNC_OFFSET(i)) + 0x0)
#define     QDMA_FUNC_QCONF_QBASE_MASK			GENMASK(31, 16)
#define     QDMA_FUNC_QCONF_NUMQ_MASK			GENMASK(15, 0)
#define QDMA_FUNC_OFFSET_INDIR_TABLE(i, k)		((QDMA_FUNC_OFFSET(i)) + 0x400 + ((k) * 4))
#define QDMA_FUNC_OFFSET_HASH_KEY(i, k)			((QDMA_FUNC_OFFSET(i)) + 0x600 + ((k) * 4))



#endif
