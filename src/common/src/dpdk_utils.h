/*
 * Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of NVIDIA CORPORATION &
 * AFFILIATES (the "Company") and all right, title, and interest in and to the
 * software product, including all associated intellectual property rights, are
 * and shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */

#ifndef COMMON_DPDK_UTILS_H_
#define COMMON_DPDK_UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <rte_mbuf.h>
#include <rte_flow.h>

#include <doca_error.h>

#include "offload_rules.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RX_RING_SIZE 1024       /* RX ring size */
#define TX_RING_SIZE 1024       /* TX ring size */
#define NUM_MBUFS (8 * 1024)    /* Number of mbufs to be allocated in the mempool */
#define MBUF_CACHE_SIZE 250     /* mempool cache size */

/*
 * Initialize dpdk environment
 *
 * @argc [in]: number of program command line arguments
 * @argv [in]: program command line arguments
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t dpdk_init(int argc, char **argv);

/*
 * destroy dpdk environment
 */
void dpdk_fini();

/*
 * Initialize dpdk ports and queues
 *
 * @app_dpdk_config [in/out]: application dpdk config struct
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t dpdk_queues_and_ports_init(struct application_dpdk_config *app_dpdk_config);

/*
 * destroy dpdk ports and queues
 *
 * @app_dpdk_config [in]: application dpdk config struct
 */
void dpdk_queues_and_ports_fini(struct application_dpdk_config *app_dpdk_config);

/*
 * Print packet header information
 *
 * @packet [in]: packet mbuf
 * @l2 [in]: if true the function prints l2 header
 * @l3 [in]: if true the function prints l3 header
 * @l4 [in]: if true the function prints l4 header
 */
void print_header_info(const struct rte_mbuf *packet, const bool l2, const bool l3, const bool l4);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* COMMON_DPDK_UTILS_H_ */
