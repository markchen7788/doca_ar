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

#ifndef COMMON_UTILS_H_
#define COMMON_UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <doca_error.h>
#include <doca_types.h>

#ifndef MIN
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))	/* Return the minimum value between X and Y */
#endif

#ifndef MAX
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))	/* Return the maximum value between X and Y */
#endif

/*
 * Prints DOCA SDK and runtime versions
 *
 * @param [in]: unused
 * @doca_config [in]: unused
 * @return: the function exit with EXIT_SUCCESS
 */
doca_error_t sdk_version_callback(void *param, void *doca_config);

/*
 * Parse string pci address into bdf struct
 *
 * @pci_addr [in]: PCI address string
 * @out_bdf [out]: doca_pci_bdf struct with the parsed string
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t parse_pci_addr(char const *pci_addr, struct doca_pci_bdf *out_bdf);

/*
 * Read the entire content of a file into a buffer
 *
 * @path [in]: file path
 * @out_bytes [out]: file data buffer
 * @out_bytes_len [out]: file length
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t read_file(char const *path, char **out_bytes, size_t *out_bytes_len);

/*
 * 64-bit extensions to regular host-to-network/network-to-host functions
 *
 * @value [in]: value to convert
 * @return: host byte order/network byte order
 */
uint64_t ntohq(uint64_t value);
#define htonq ntohq

#endif /* COMMON_UTILS_H_ */
