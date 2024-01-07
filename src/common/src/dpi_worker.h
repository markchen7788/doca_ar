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

#ifndef COMMON_DPI_WORKER_H_
#define COMMON_DPI_WORKER_H_

#include <doca_dpi.h>

#include "telemetry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* DPI worker action to take when a match is found */
enum dpi_worker_action {
	DPI_WORKER_ALLOW,	/* Allow the flow to pass and offload to HW */
	DPI_WORKER_DROP,	/* Drop the flow and offload to HW */
	DPI_WORKER_RSS_FLOW	/* Allow the flow to pass without offloading to HW */
};

/* Callback function to be called when DPI engine matches a packet */
typedef int (*dpi_on_match_t)(int queue,
				const struct doca_dpi_result *result,
				uint32_t fid,
				void *user_data,
				enum dpi_worker_action *dpi_action);

/* Callback function to be called to send netflow records */
typedef void (*send_netflow_record_t)(const struct doca_telemetry_netflow_record *record);

/* DPI worker attributes */
struct dpi_worker_attr {
	dpi_on_match_t		dpi_on_match;		/* Will be called on DPI match */
	send_netflow_record_t	send_netflow_record;	/* Will be called when netflow record is ready to be sent */
	struct doca_dpi_ctx	*dpi_ctx;		/* DOCA DPI context, passed to all workers */
	void			*user_data;		/* User data passed to dpi_on_match */
	uint64_t		max_dpi_depth;		/* Max DPI depth search limit, use 0 for unlimited depth. */
};

/*
 * Prints DPI signature status
 *
 * @dpi_ctx [in]: DOCA DPI context
 * @sig_id [in]: DPI signature ID
 * @fid [in]: Flow ID
 * @blocked [in]: 1 if signature is blocked and 0 otherwise
 */
void printf_signature(struct doca_dpi_ctx *dpi_ctx, uint32_t sig_id, uint32_t fid, bool blocked);

/*
 * This is the main worker calling function, each queue represents a core
 *
 * @available_cores [in]: Number of available cores
 * @client_id [in]: Client ID
 * @attr [in]: DPI worker attributes
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t dpi_worker_lcores_run(int available_cores, int client_id, struct dpi_worker_attr attr);

/*
 * Stops lcores and wait until all lcores are stopped.
 *
 * @dpi_ctx [in]: DPI context
 */
void dpi_worker_lcores_stop(struct doca_dpi_ctx *dpi_ctx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* COMMON_DPI_WORKER_H_ */
