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

#ifndef COMMON_SIG_DB_H_
#define COMMON_SIG_DB_H_

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include <doca_error.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SIG_APP_NAME 64     /* Maximal length of application name */

/*
 * Signature data structure
 */
struct sig_info {
        uint32_t sig_id;                        /* Signature ID */
        char app_name[MAX_SIG_APP_NAME];        /* Application name */
        struct tm timestamp;                    /* Timestamp */
        uint32_t num_fids;                      /* Total number of Flow IDs that matched this signature */
        bool block;                             /* Whether this signature is blocked or not */
};

/*
 * Returns whether the flow is blocked or not
 *
 * @sig_id [in]: Signature ID
 * @status [out]: 1 if blocked, 0 otherwise
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t sig_db_sig_info_get_block_status(uint32_t sig_id, bool *status);

/*
 * Set new block status for the given signature ID
 *
 * @sig_id [in]: Signature ID
 * @block [in]: block status
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t sig_db_sig_info_set_block_status(uint32_t sig_id, bool block);

/*
 * Increase number of Flow IDs that matched this signature
 *
 * @sig_id [in]: Signature ID
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t sig_db_sig_info_fids_inc(uint32_t sig_id);

/*
 * Update application name for the given signature ID
 *
 * @sig_id [in]: Signature ID
 * @app_name [in]: Application name
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t sig_db_sig_info_set(uint32_t sig_id, char *app_name);

/*
 * Get signature data for the given signature ID
 *
 * @sig_id [in]: Signature ID
 * @data [out]: Signature data
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t sig_db_sig_info_get(uint32_t sig_id, struct sig_info **data);

/*
 * Create new signature data entry in the hash table
 *
 * @sig_id [in]: Signature ID
 * @app_name [in]: Application name
 * @block [in]: block status
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t sig_db_sig_info_create(uint32_t sig_id, char *app_name, bool block);

/*
 * Initialize signature database
 *
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t sig_db_init(void);

/*
 * Destroy the signature database
 */
void sig_db_destroy(void);

/*
 * Dumps the signature database to the given file as CSV format
 *
 * @csv_filename [in]: CSV file name
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t sig_database_write_to_csv(void *csv_filename);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* COMMON_SIG_DB_H_ */
