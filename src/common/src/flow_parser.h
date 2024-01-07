/*
 * Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
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

#ifndef COMMON_FLOW_PARSER_H_
#define COMMON_FLOW_PARSER_H_

#include <doca_flow.h>

/*
 * Parse IPv4 string
 *
 * @str_ip [in]: String to parse
 * @ipv4_addr [out]: Big endian IPv4 address
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t parse_ipv4_str(const char *str_ip, doca_be32_t *ipv4_addr);

/*
 * Parse network layer protocol
 *
 * @protocol_str [in]: String to parse
 * @protocol [out]: Protocol identifier number
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t parse_protocol_string(const char *protocol_str, uint8_t *protocol);

/*
 * Set the function to be called once pipe create command is entered
 *
 * @action [in]: Function callback
 */
void set_pipe_create(void (*action)(struct doca_flow_pipe_cfg *cfg, uint16_t port_id,
						struct doca_flow_fwd *fwd, uint64_t fw_pipe_id,
						struct doca_flow_fwd *fwd_miss, uint64_t fw_miss_pipe_id));

/*
 * Set the function to be called once add entry command is entered
 *
 * @action [in]: Function callback
 */
void set_pipe_add_entry(void (*action)(uint16_t pipe_queue, uint64_t pipe_id,
						 struct doca_flow_match *match, struct doca_flow_actions *actions,
						 struct doca_flow_monitor *monitor,
						 struct doca_flow_fwd *fwd, uint64_t fw_pipe_id,
						 uint32_t flags));

/*
 * Set the function to be called once pipe control add entry command is entered
 *
 * @action [in]: Function callback
 */
void set_pipe_control_add_entry(void (*action)(uint16_t pipe_queue, uint8_t priority, uint64_t pipe_id,
						 struct doca_flow_match *match, struct doca_flow_match *match_mask,
						 struct doca_flow_fwd *fwd, uint64_t fw_pipe_id));

/*
 * Set the function to be called once pipe destroy command is entered
 *
 * @action [in]: Function callback
 */
void set_pipe_destroy(void (*action)(uint64_t pipe_id));

/*
 * Set the function to be called once remove entry command is entered
 *
 * @action [in]: Function callback
 */
void set_pipe_rm_entry(void (*action)(uint16_t pipe_queue, uint64_t entry_id));

/*
 * Set the function to be called once pipes flush command is entered
 *
 * @action [in]: Function callback
 */
void set_port_pipes_flush(void (*action)(uint16_t port_id));

/*
 * Set the function to be called once set query command is entered
 *
 * @action [in]: Function callback
 */
void set_query(void (*action)(uint64_t entry_id, struct doca_flow_query *states));

/*
 * Set the function to be called once port pipes dump command is entered
 *
 * @action [in]: Function callback
 */
void set_port_pipes_dump(void (*action)(uint16_t port_id, FILE *fd));

/*
 * Initialize parser and open the command line interface
 *
 * @shell_prompt [in]: String for the shell to prompt
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t flow_parser_init(char *shell_prompt);

/*
 * Destroy flow parser structures
 */
void flow_parser_cleanup(void);

#endif /* COMMON_FLOW_PARSER_H_ */
