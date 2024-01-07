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

#ifndef COMMON_TELEMETRY_H_
#define COMMON_TELEMETRY_H_

#include <stdio.h>
#include <string.h>

#include <rte_ring.h>

#include <doca_telemetry_netflow.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NETFLOW_QUEUE_SIZE 1024		/* Netflow queue size */

/* Netflow record, should match the fields initialized in doca_telemetry_netflow_init */
struct __attribute__((packed)) doca_telemetry_netflow_record {
	__be32 src_addr_v4;	     /* Source IPV4 Address */
	__be32 dst_addr_v4;	     /* Destination IPV4 Address */
	struct in6_addr src_addr_v6; /* Source IPV6 Address */
	struct in6_addr dst_addr_v6; /* Destination IPV6 Address */
	__be32 next_hop_v4;	     /* Next hop router's IPV4 Address */
	struct in6_addr next_hop_v6; /* Next hop router's IPV6 Address */
	__be16 input;		     /* Input interface index */
	__be16 output;		     /* Output interface index */
	__be16 src_port;	     /* TCP/UDP source port number or equivalent */
	__be16 dst_port;	     /* TCP/UDP destination port number or equivalent */
	uint8_t tcp_flags;	     /* Cumulative OR of tcp flags */
	uint8_t protocol;	     /* IP protocol type (for example, TCP = 6;UDP = 17) */
	uint8_t tos;		     /* IP Type-of-Service */
	__be16 src_as;		     /* originating AS of source address */
	__be16 dst_as;		     /* originating AS of destination address */
	uint8_t src_mask;	     /* source address prefix mask bits */
	uint8_t dst_mask;	     /* destination address prefix mask bits */
	__be32 d_pkts;		     /* Packets sent in Duration */
	__be32 d_octets;	     /* Octets sent in Duration. */
	__be32 first;		     /* SysUptime at start of flow */
	__be32 last;		     /* and of last packet of flow */
	__be64 flow_id;		     /* This identifies a transaction within a connection */
	char application_name[DOCA_TELEMETRY_NETFLOW_APPLICATION_NAME_DEFAULT_LENGTH]; /* Name associated with a classification*/
};

/*
 * Send Netflow records available in the pending queue
 *
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t send_netflow_record(void);

/*
 * Enqueues a single Netflow record to the pending queue
 * This function can be used as callback function to the DPI Worker and is MP safe
 *
 * @record [in]: Netflow record to be enqueued
 */
void enqueue_netflow_record_to_ring(const struct doca_telemetry_netflow_record *record);

/*
 * Destroy the Netflow telemetry resources
 */
void destroy_netflow_schema_and_source(void);

/*
 * Initialize the Netflow telemetry resources
 *
 * @id [in]: Netflow source id
 * @source_tag [in]: Netflow source tag
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t init_netflow_schema_and_source(uint8_t id, char *source_tag);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* COMMON_TELEMETRY_H_ */
