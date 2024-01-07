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

#include <bsd/string.h>
#include <inttypes.h>
#include <string.h>

#include <cmdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <rte_byteorder.h>

#include <doca_log.h>

#include "flow_parser.h"

DOCA_LOG_REGISTER(FLOW_PARSER);

#define MAX_CMDLINE_INPUT_LEN 512			/* Maximum size of input command  */
#define MAC_ADDR_LEN 6					/* MAC address size in bytes */
#define IP_ADDR_LEN 4					/* IP address size in bytes */
#define MAX_FIELD_INPUT_LEN 128				/* Maximum size of field input */
#define NAME_STR_LEN 5					/* Name string size */
#define FWD_STR_LEN 4					/* Forward string size */
#define MISS_FWD_STR_LEN 9				/* Forward miss string size */
#define MATCH_MASK_STR_LEN 11				/* Match mask string size */
#define MONITOR_STR_LEN 8				/* Monitor string size */
#define ROOT_ENABLE_STR_LEN 12				/* Root enable string size */
#define PORT_ID_STR_LEN 8				/* Port ID string size */
#define PIPE_ID_STR_LEN 8				/* Pipe ID string size */
#define ENTRY_ID_STR_LEN 9				/* Entry ID string size */
#define PIPE_QUEUE_STR_LEN 11				/* Pipe queue string size */
#define PRIORITY_STR_LEN 9				/* Priority string size */
#define FILE_STR_LEN 5					/* File string size */
#define TYPE_STR_LEN 5					/* Type enable string size */
#define HEXADECIMAL_BASE 1				/* Hex base */
#define UINT32_CHANGEABLE_FIELD "0xffffffff"		/* DOCA flow masking for 32 bits value */

#define BE_IPV4_ADDR(a, b, c, d) (RTE_BE32((a << 24) + (b << 16) + (c << 8) + d))	/* Big endian conversion */

static void (*destroy_pipe_func)(uint64_t);				/* Callback for destroy pipe command */
static void (*remove_entry_func)(uint16_t, uint64_t);			/* Callback for remove entry command */
static void (*port_pipes_flush_func)(uint16_t);				/* Callback for port pipes flush command */
static void (*query_func)(uint64_t, struct doca_flow_query *);		/* Callback query command */
static void (*port_pipes_dump_func)(uint16_t, FILE *);			/* Callback for port pipes dump command */
static void (*create_pipe_func)(struct doca_flow_pipe_cfg *, uint16_t, struct doca_flow_fwd *, uint64_t,
				struct doca_flow_fwd *, uint64_t);	/* Callback for create pipe command */
static void (*add_entry_func)(uint16_t, uint64_t, struct doca_flow_match *, struct doca_flow_actions *,
			      struct doca_flow_monitor *, struct doca_flow_fwd *, uint64_t,
			      uint32_t);				/* Callback for add entry command */
static void (*add_control_pipe_entry_func)(uint16_t, uint8_t, uint64_t, struct doca_flow_match *,
					   struct doca_flow_match *, struct doca_flow_fwd *,
					   uint64_t);			/* Callback for add control pipe entry command */


static struct doca_flow_match pipe_match;			/* DOCA Flow pipe match structure */
static struct doca_flow_match entry_match;			/* DOCA Flow entry match structure */
static struct doca_flow_match match_mask;			/* DOCA Flow match mask structure */
static struct doca_flow_actions actions;			/* DOCA Flow actions structure */
static struct doca_flow_monitor monitor;			/* DOCA Flow monitor structure */
static struct doca_flow_fwd fwd;				/* DOCA Flow forward structure */
static struct doca_flow_fwd fwd_miss;				/* DOCA Flow forward miss structure */
static uint16_t pipe_port_id;					/* DOCA Flow pipe port id */
static uint64_t fwd_next_pipe_id;				/* DOCA Flow next fwd pipe id */
static uint64_t fwd_miss_next_pipe_id;				/* DOCA Flow next miss fwd pipe id */
static uint16_t *rss_queues;					/* DOCA Flow RSS queues */

/* Create pipe command result */
struct cmd_create_pipe_result {
	cmdline_fixed_string_t create;	/* Command first segment */
	cmdline_fixed_string_t pipe;	/* Command second segment */
	cmdline_fixed_string_t params;	/* Command last segment */
};

/* Add entry command result */
struct cmd_add_entry_result {
	cmdline_fixed_string_t add;	/* Command first segment */
	cmdline_fixed_string_t entry;	/* Command second segment */
	cmdline_fixed_string_t params;	/* Command last segment */
};

/* Add control pipe command result */
struct cmd_add_control_pipe_entry_result {
	cmdline_fixed_string_t add;		/* Command first segment */
	cmdline_fixed_string_t control_pipe;	/* Command second segment */
	cmdline_fixed_string_t entry;		/* Command third segment */
	cmdline_fixed_string_t params;		/* Command last segment */
};

/* Destroy pipe command result */
struct cmd_destroy_pipe_result {
	cmdline_fixed_string_t destroy;		/* Command first segment */
	cmdline_fixed_string_t pipe;		/* Command second segment */
	cmdline_fixed_string_t params;		/* Command last segment */
};

/* Remove entry command result */
struct cmd_rm_entry_result {
	cmdline_fixed_string_t rm;		/* Command first segment */
	cmdline_fixed_string_t entry;		/* Command second segment */
	cmdline_fixed_string_t params;		/* Command last segment */
};

/* Flush pipes command result */
struct cmd_flush_pipes_result {
	cmdline_fixed_string_t port;		/* Command first segment */
	cmdline_fixed_string_t pipes;		/* Command second segment */
	cmdline_fixed_string_t flush;		/* Command third segment */
	cmdline_fixed_string_t port_id;		/* Command last segment */
};

/* Query command result */
struct cmd_query_result {
	cmdline_fixed_string_t query;		/* Command first segment */
	cmdline_fixed_string_t params;		/* Command second segment */
};

/* Dump pipe command result */
struct cmd_dump_pipe_result {
	cmdline_fixed_string_t port;		/* Command first segment */
	cmdline_fixed_string_t pipes;		/* Command second segment */
	cmdline_fixed_string_t dump;		/* Command third segment */
	cmdline_fixed_string_t params;		/* Command last segment */
};

/* Create flow structure command result */
struct cmd_create_struct_result {
	cmdline_fixed_string_t create;			/* Command first segment */
	cmdline_fixed_string_t flow_struct;		/* Command second segment */
	cmdline_multi_string_t flow_struct_input;	/* Command last segment */
};

/* Quit command result */
struct cmd_quit_result {
	cmdline_fixed_string_t quit;	/* Command first segment */
};

void
set_pipe_create(void (*action)(struct doca_flow_pipe_cfg *, uint16_t, struct doca_flow_fwd *, uint64_t,
			       struct doca_flow_fwd *, uint64_t))
{
	create_pipe_func = action;
}

void
set_pipe_add_entry(void (*action)(uint16_t, uint64_t, struct doca_flow_match *, struct doca_flow_actions *,
				  struct doca_flow_monitor *, struct doca_flow_fwd *, uint64_t, uint32_t))
{
	add_entry_func = action;
}

void
set_pipe_control_add_entry(void (*action)(uint16_t, uint8_t, uint64_t, struct doca_flow_match *,
					  struct doca_flow_match *, struct doca_flow_fwd *, uint64_t))
{
	add_control_pipe_entry_func = action;
}

void
set_pipe_destroy(void (*action)(uint64_t))
{
	destroy_pipe_func = action;
}

void
set_pipe_rm_entry(void (*action)(uint16_t, uint64_t))
{
	remove_entry_func = action;
}

void
set_port_pipes_flush(void (*action)(uint16_t))
{
	port_pipes_flush_func = action;
}

void
set_query(void (*action)(uint64_t, struct doca_flow_query *))
{
	query_func = action;
}

void
set_port_pipes_dump(void (*action)(uint16_t, FILE *))
{
	port_pipes_dump_func = action;
}

/*
 * Reset DOCA Flow structures
 */
static void
reset_doca_flow_structs()
{
	memset(&pipe_match, 0, sizeof(pipe_match));
	memset(&entry_match, 0, sizeof(entry_match));
	memset(&match_mask, 0, sizeof(match_mask));
	memset(&actions, 0, sizeof(actions));
	memset(&monitor, 0, sizeof(monitor));
	memset(&fwd, 0, sizeof(fwd));
	memset(&fwd_miss, 0, sizeof(fwd_miss));
}

doca_error_t
parse_ipv4_str(const char *str_ip, doca_be32_t *ipv4_addr)
{
	char *ptr;
	int i;
	int ips[4];

	if (strcmp(str_ip, UINT32_CHANGEABLE_FIELD) == 0) {
		*ipv4_addr = UINT32_MAX;
		return DOCA_SUCCESS;
	}
	for (i = 0; i < 3; i++) {
		ips[i] = atoi(str_ip);
		ptr = strchr(str_ip, '.');
		if (ptr == NULL) {
			DOCA_LOG_ERR("Wrong format of ip string");
			return DOCA_ERROR_INVALID_VALUE;
		}
		str_ip = ++ptr;
	}
	ips[3] = atoi(ptr);
	*ipv4_addr = BE_IPV4_ADDR(ips[0], ips[1], ips[2], ips[3]);
	return DOCA_SUCCESS;
}

doca_error_t
parse_protocol_string(const char *protocol_str, uint8_t *protocol)
{
	if (strcmp(protocol_str, "tcp") == 0)
		*protocol = DOCA_PROTO_TCP;
	else if (strcmp(protocol_str, "udp") == 0)
		*protocol = DOCA_PROTO_UDP;
	else if (strcmp(protocol_str, "gre") == 0)
		*protocol = DOCA_PROTO_GRE;
	else {
		DOCA_LOG_ERR("protocol type %s is not supported", protocol_str);
		return DOCA_ERROR_INVALID_VALUE;
	}
	return DOCA_SUCCESS;
}

/*
 * Parse pipe id
 *
 * @pipe_id_str [in]: String to parse from
 * @pipe_id [out]: Pipe id
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_pipe_id_input(char *pipe_id_str, uint64_t *pipe_id)
{
	if (strncmp(pipe_id_str, "pipe_id=", PIPE_ID_STR_LEN) != 0) {
		DOCA_LOG_ERR("Wrong format of pipe id string: \'pipe_id=<pipe_id>\'");
		return DOCA_ERROR_INVALID_VALUE;
	}
	pipe_id_str += PIPE_ID_STR_LEN;
	*pipe_id = strtoull(pipe_id_str, NULL, 0);
	return DOCA_SUCCESS;
}

/*
 * Parse port id
 *
 * @port_id_str [in]: String to parse from
 * @port_id [out]: Port id
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_port_id_input(const char *port_id_str, int *port_id)
{
	if (strncmp(port_id_str, "port_id=", PORT_ID_STR_LEN) != 0) {
		DOCA_LOG_ERR("Wrong format of port id string: \'port_id=<port_id>\'");
		return DOCA_ERROR_INVALID_VALUE;
	}
	port_id_str += PORT_ID_STR_LEN;
	*port_id = strtol(port_id_str, NULL, 0);
	return DOCA_SUCCESS;
}

/*
 * Parse tunnel type
 *
 * @tun_type_str [in]: String to parse from
 * @tun_type [out]: Tunnel type
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_tun_type_string(const char *tun_type_str, enum doca_flow_tun_type *tun_type)
{
	if (strcmp(tun_type_str, "vxlan") == 0)
		*tun_type = DOCA_FLOW_TUN_VXLAN;
	else if (strcmp(tun_type_str, "gtpu") == 0)
		*tun_type = DOCA_FLOW_TUN_GTPU;
	else if (strcmp(tun_type_str, "gre") == 0)
		*tun_type = DOCA_FLOW_TUN_GRE;
	else {
		DOCA_LOG_ERR("tun type %s is not supported", tun_type_str);
		return DOCA_ERROR_INVALID_VALUE;
	}
	return DOCA_SUCCESS;
}

/*
 * Parse forward type
 *
 * @fwd_str [in]: String to parse from
 * @fwd [out]: Forward type
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_fwd_type(const char *fwd_str, enum doca_flow_fwd_type *fwd)
{
	if (strcmp(fwd_str, "rss") == 0)
		*fwd = DOCA_FLOW_FWD_RSS;
	else if (strcmp(fwd_str, "port") == 0)
		*fwd = DOCA_FLOW_FWD_PORT;
	else if (strcmp(fwd_str, "pipe") == 0)
		*fwd = DOCA_FLOW_FWD_PIPE;
	else if (strcmp(fwd_str, "drop") == 0)
		*fwd = DOCA_FLOW_FWD_DROP;
	else {
		DOCA_LOG_ERR("fwd type %s is not supported", fwd_str);
		return DOCA_ERROR_INVALID_VALUE;
	}
	return DOCA_SUCCESS;
}

/*
 * Parse pipe type
 *
 * @pipe_type [in]: String to parse from
 * @type [out]: Pipe type
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_pipe_type(const char *pipe_type, enum doca_flow_pipe_type *type)
{
	if (strcmp(pipe_type, "basic") == 0)
		*type = DOCA_FLOW_PIPE_BASIC;
	else if (strcmp(pipe_type, "control") == 0)
		*type = DOCA_FLOW_PIPE_CONTROL;
	else {
		DOCA_LOG_ERR("pipe type %s is not supported", pipe_type);
		return DOCA_ERROR_INVALID_VALUE;
	}
	return DOCA_SUCCESS;
}

/*
 * Parse ip type
 *
 * @ip_type_str [in]: String to parse from
 * @ip_type [out]: Ip type
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_ip_type(const char *ip_type_str, uint8_t *ip_type)
{
	if (strcmp(ip_type_str, "ipv4") == 0)
		*ip_type = DOCA_FLOW_IP4_ADDR;
	else if (strcmp(ip_type_str, "ipv6") == 0)
		*ip_type = DOCA_FLOW_IP6_ADDR;
	else {
		DOCA_LOG_ERR("ip type %s is not supported", ip_type_str);
		return DOCA_ERROR_INVALID_VALUE;
	}
	return DOCA_SUCCESS;
}

/*
 * Parse TCP flag
 *
 * @tcp_flag_str [in]: String to parse from
 * @tcp_flag [out]: TCP flag
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_tcp_flag_string(const char *tcp_flag_str, uint8_t *tcp_flag)
{
	if (strcmp(tcp_flag_str, "FIN") == 0)
		*tcp_flag = DOCA_FLOW_MATCH_TCP_FLAG_FIN;
	else if (strcmp(tcp_flag_str, "SYN") == 0)
		*tcp_flag = DOCA_FLOW_MATCH_TCP_FLAG_SYN;
	else if (strcmp(tcp_flag_str, "RST") == 0)
		*tcp_flag = DOCA_FLOW_MATCH_TCP_FLAG_RST;
	else if (strcmp(tcp_flag_str, "PSH") == 0)
		*tcp_flag = DOCA_FLOW_MATCH_TCP_FLAG_PSH;
	else if (strcmp(tcp_flag_str, "ACK") == 0)
		*tcp_flag = DOCA_FLOW_MATCH_TCP_FLAG_ACK;
	else if (strcmp(tcp_flag_str, "URG") == 0)
		*tcp_flag = DOCA_FLOW_MATCH_TCP_FLAG_URG;
	else if (strcmp(tcp_flag_str, "ECE") == 0)
		*tcp_flag = DOCA_FLOW_MATCH_TCP_FLAG_ECE;
	else if (strcmp(tcp_flag_str, "CWR") == 0)
		*tcp_flag = DOCA_FLOW_MATCH_TCP_FLAG_CWR;
	else {
		DOCA_LOG_ERR("tcp flag %s is not supported", tcp_flag_str);
		return DOCA_ERROR_INVALID_VALUE;
	}
	return DOCA_SUCCESS;
}

/*
 * Parse MAC address
 *
 * @mac_addr_str [in]: String to parse from
 * @mac_addr [out]: MAC address
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_mac_address(char *mac_addr_str, uint8_t *mac_addr)
{
	char *ptr;
	int i;

	for (i = 0; i < MAC_ADDR_LEN - 1; i++) {
		mac_addr[i] = strtol(mac_addr_str, NULL, HEXADECIMAL_BASE);
		ptr = strchr(mac_addr_str, ':');
		if (ptr)
			mac_addr_str = ++ptr;
		else {
			DOCA_LOG_ERR("Wrong format of mac address");
			return DOCA_ERROR_INVALID_VALUE;
		}
	}
	mac_addr[MAC_ADDR_LEN - 1] = strtol(ptr, NULL, HEXADECIMAL_BASE);
	return DOCA_SUCCESS;
}

/*
 * Parse IPv6 address
 *
 * @str_ip [in]: String to parse from
 * @ipv6_addr [out]: IPv6 address
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_ipv6_str(const char *str_ip, doca_be32_t *ipv6_addr)
{
	char *ptr;
	int i;
	int j;

	for (i = 0; i < IP_ADDR_LEN; i++) {
		int ips[2];

		for (j = 0; j < 2; j++) {
			ips[j] = strtol(str_ip, &ptr, HEXADECIMAL_BASE);
			if (ptr)
				str_ip = ++ptr;
			else {
				DOCA_LOG_ERR("Wrong format of ip string");
				return DOCA_ERROR_INVALID_VALUE;
			}
		}
		ipv6_addr[i] = RTE_BE32((ips[0] << 16) + ips[1]);
	}
	return DOCA_SUCCESS;
}

/*
 * Parse rss queues
 *
 * @rss_queues_str [in]: String to parse from
 * @num_of_queues [in]: Number of queues
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_rss_queues(char *rss_queues_str, int num_of_queues)
{
	int i;

	if (rss_queues)
		free(rss_queues);
	rss_queues = malloc(sizeof(uint16_t) * num_of_queues);
	if (rss_queues == NULL) {
		DOCA_LOG_ERR("failed to allocate rss queues");
		return DOCA_ERROR_NO_MEMORY;
	}

	for (i = 0; i < num_of_queues - 1; i++) {
		rss_queues[i] = strtol(rss_queues_str, NULL, 0);
		rss_queues_str = rss_queues_str + 2;
	}
	rss_queues[num_of_queues - 1] = strtol(rss_queues_str, NULL, 0);
	return DOCA_SUCCESS;
}

/*
 * Parse boolean parameter
 *
 * @bool_str [in]: String to parse from
 * @bool_val [out]: Boolean value
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_bool_string(char *bool_str, bool *bool_val)
{
	if (strcmp(bool_str, "true") == 0)
		*bool_val = true;
	else if (strcmp(bool_str, "false") == 0)
		*bool_val = false;
	else {
		DOCA_LOG_ERR("bool type must be true or false");
		return DOCA_ERROR_INVALID_VALUE;
	}
	return DOCA_SUCCESS;
}

/*
 * Parse DOCA Flow monitor field
 *
 * @field_name [in]: Field to parse
 * @value [in]: Value to read
 * @struct_ptr [out]: Monitor struct to fill
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_monitor_field(char *field_name, char *value, void *struct_ptr)
{
	struct doca_flow_monitor *monitor = (struct doca_flow_monitor *)struct_ptr;

	if (strcmp(field_name, "flags") == 0)
		monitor->flags = (uint8_t)strtol(value, NULL, 0);

	else if (strcmp(field_name, "cir") == 0)
		monitor->cir = strtoull(value, NULL, 0);

	else if (strcmp(field_name, "cbs") == 0)
		monitor->cbs = strtoull(value, NULL, 0);

	else if (strcmp(field_name, "aging") == 0)
		monitor->aging = strtol(value, NULL, 0);

	else {
		DOCA_LOG_ERR("%s is not supported field in monitor", field_name);
		return DOCA_ERROR_INVALID_VALUE;
	}
	return DOCA_SUCCESS;
}

/*
 * Parse DOCA Flow forward field
 *
 * @field_name [in]: Field to parse
 * @value [in]: Value to read
 * @struct_ptr [out]: Forward struct to fill
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_fwd_field(char *field_name, char *value, void *struct_ptr)
{
	doca_error_t result;
	struct doca_flow_fwd *fwd = (struct doca_flow_fwd *)struct_ptr;

	if (strcmp(field_name, "type") == 0) {
		result = parse_fwd_type(value, &fwd->type);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "rss_flags") == 0)
		fwd->rss_flags = strtol(value, NULL, 0);

	else if (strcmp(field_name, "rss_queues") == 0) {
		result = parse_rss_queues(value, fwd->num_of_queues);
		if (result != DOCA_SUCCESS)
			return result;
		fwd->rss_queues = rss_queues;
	} else if (strcmp(field_name, "num_of_queues") == 0)
		fwd->num_of_queues = strtol(value, NULL, 0);

	else if (strcmp(field_name, "port_id") == 0)
		fwd->port_id = strtol(value, NULL, 0);

	else if (strcmp(field_name, "next_pipe_id") == 0)
		fwd_next_pipe_id = strtoull(value, NULL, 0);

	else {
		DOCA_LOG_ERR("%s is not supported field in fwd", field_name);
		return DOCA_ERROR_INVALID_VALUE;
	}
	return DOCA_SUCCESS;
}

/*
 * Parse DOCA Flow forward miss field
 *
 * @field_name [in]: Field to parse
 * @value [in]: Value to read
 * @struct_ptr [out]: Forward miss struct to fill
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_fwd_miss_field(char *field_name, char *value, void *struct_ptr)
{
	doca_error_t result;
	struct doca_flow_fwd *fwd_miss = (struct doca_flow_fwd *)struct_ptr;

	if (strcmp(field_name, "type") == 0) {
		result = parse_fwd_type(value, &fwd_miss->type);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "rss_flags") == 0)
		fwd_miss->rss_flags = strtol(value, NULL, 0);

	else if (strcmp(field_name, "rss_queues") == 0) {
		result = parse_rss_queues(value, fwd_miss->num_of_queues);
		if (result != DOCA_SUCCESS)
			return result;
		fwd_miss->rss_queues = rss_queues;
	} else if (strcmp(field_name, "num_of_queues") == 0)
		fwd_miss->num_of_queues = strtol(value, NULL, 0);

	else if (strcmp(field_name, "port_id") == 0)
		fwd_miss->port_id = strtol(value, NULL, 0);

	else if (strcmp(field_name, "next_pipe_id") == 0)
		fwd_miss_next_pipe_id = strtoull(value, NULL, 0);

	else {
		DOCA_LOG_ERR("%s is not supported field in fwd_miss", field_name);
		return DOCA_ERROR_INVALID_VALUE;
	}
	return DOCA_SUCCESS;
}

/*
 * Parse DOCA Flow actions field
 *
 * @field_name [in]: Field to parse
 * @value [in]: Value to read
 * @struct_ptr [out]: Actions struct to fill
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_actions_field(char *field_name, char *value, void *struct_ptr)
{
	doca_error_t result;
	struct doca_flow_actions *action = (struct doca_flow_actions *)struct_ptr;

	if (strcmp(field_name, "decap") == 0) {
		result = parse_bool_string(value, &action->decap);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "mod_src_mac") == 0) {
		result = parse_mac_address(value, action->mod_src_mac);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "mod_dst_mac") == 0) {
		result = parse_mac_address(value, action->mod_dst_mac);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "mod_src_ip_type") == 0) {
		result = parse_ip_type(value, &action->mod_src_ip.type);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "mod_src_ip_addr") == 0) {
		if (action->mod_src_ip.type == DOCA_FLOW_IP4_ADDR) {
			result = parse_ipv4_str(value, &action->mod_src_ip.ipv4_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else if (action->mod_src_ip.type == DOCA_FLOW_IP6_ADDR) {
			result = parse_ipv6_str(value, action->mod_src_ip.ipv6_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else {
			DOCA_LOG_ERR("src ip type is not set, need to set ip type before address");
			return DOCA_ERROR_INVALID_VALUE;
		}
	} else if (strcmp(field_name, "mod_dst_ip_type") == 0) {
		result = parse_ip_type(value, &action->mod_dst_ip.type);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "mod_dst_ip_addr") == 0) {
		if (action->mod_dst_ip.type == DOCA_FLOW_IP4_ADDR) {
			result = parse_ipv4_str(value, &action->mod_dst_ip.ipv4_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else if (action->mod_dst_ip.type == DOCA_FLOW_IP6_ADDR) {
			result = parse_ipv6_str(value, action->mod_dst_ip.ipv6_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else {
			DOCA_LOG_ERR("dst ip type is not set, need to set ip type before address");
			return DOCA_ERROR_INVALID_VALUE;
		}
	} else if (strcmp(field_name, "mod_src_port") == 0)
		action->mod_src_port = strtol(value, NULL, 0);

	else if (strcmp(field_name, "mod_dst_port") == 0)
		action->mod_dst_port = strtol(value, NULL, 0);

	else if (strcmp(field_name, "ttl") == 0)
		action->ttl = strtol(value, NULL, 0);

	else if (strcmp(field_name, "has_encap") == 0) {
		result = parse_bool_string(value, &action->has_encap);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "encap_src_mac") == 0) {
		result = parse_mac_address(value, action->encap.src_mac);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "encap_dst_mac") == 0) {
		result = parse_mac_address(value, action->encap.dst_mac);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "encap_src_ip_type") == 0) {
		result = parse_ip_type(value, &action->encap.src_ip.type);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "encap_src_ip_addr") == 0) {
		if (action->encap.src_ip.type == DOCA_FLOW_IP4_ADDR) {
			result = parse_ipv4_str(value, &action->encap.src_ip.ipv4_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else if (action->encap.src_ip.type == DOCA_FLOW_IP6_ADDR) {
			result = parse_ipv6_str(value, action->encap.src_ip.ipv6_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else {
			DOCA_LOG_ERR("encap src ip type is not set, need to set ip type before address");
			return DOCA_ERROR_INVALID_VALUE;
		}
	} else if (strcmp(field_name, "encap_dst_ip_type") == 0) {
		result = parse_ip_type(value, &action->encap.dst_ip.type);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "encap_dst_ip_addr") == 0) {
		if (action->encap.dst_ip.type == DOCA_FLOW_IP4_ADDR) {
			result = parse_ipv4_str(value, &action->encap.dst_ip.ipv4_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else if (action->encap.dst_ip.type == DOCA_FLOW_IP6_ADDR) {
			result = parse_ipv6_str(value, action->encap.dst_ip.ipv6_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else {
			DOCA_LOG_ERR("encap dst ip type is not set, need to set ip type before address");
			return DOCA_ERROR_INVALID_VALUE;
		}
	} else if (strcmp(field_name, "encap_tun_type") == 0) {
		result = parse_tun_type_string(value, &action->encap.tun.type);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "encap_vxlan_tun_id") == 0)
		action->encap.tun.vxlan_tun_id = strtol(value, NULL, 0);

	else if (strcmp(field_name, "encap_gre_key") == 0)
		action->encap.tun.gre_key = strtol(value, NULL, 0);

	else if (strcmp(field_name, "encap_gtp_teid") == 0)
		action->encap.tun.gtp_teid = strtol(value, NULL, 0);

	else {
		DOCA_LOG_ERR("%s is not supported field in actions", field_name);
		return DOCA_ERROR_INVALID_VALUE;
	}
	return DOCA_SUCCESS;
}

/*
 * Parse DOCA Flow match field
 *
 * @field_name [in]: Field to parse
 * @value [in]: Value to read
 * @struct_ptr [out]: Match struct to fill
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_match_field(char *field_name, char *value, void *struct_ptr)
{
	doca_error_t result;
	struct doca_flow_match *match = (struct doca_flow_match *)struct_ptr;

	if (strcmp(field_name, "flags") == 0)
		match->flags = (uint32_t)strtol(value, NULL, HEXADECIMAL_BASE);

	else if (strcmp(field_name, "port_meta") == 0)
		match->meta.port_meta = (uint32_t)strtol(value, NULL, 0);

	else if (strcmp(field_name, "out_src_mac") == 0) {
		result = parse_mac_address(value, match->out_src_mac);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "out_dst_mac") == 0) {
		result = parse_mac_address(value, match->out_dst_mac);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "out_eth_type") == 0)
		match->out_eth_type = (uint16_t)strtol(value, NULL, HEXADECIMAL_BASE);

	else if (strcmp(field_name, "out_vlan_tci") == 0)
		match->out_vlan_tci = (uint16_t)strtol(value, NULL, 0);

	else if (strcmp(field_name, "out_src_ip_type") == 0) {
		result = parse_ip_type(value, &match->out_src_ip.type);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "out_src_ip_addr") == 0) {
		if (match->out_src_ip.type == DOCA_FLOW_IP4_ADDR) {
			result = parse_ipv4_str(value, &match->out_src_ip.ipv4_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else if (match->out_src_ip.type == DOCA_FLOW_IP6_ADDR) {
			result = parse_ipv6_str(value, match->out_src_ip.ipv6_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else {
			DOCA_LOG_ERR("src ip type is not set, need to set ip type before address");
			return DOCA_ERROR_INVALID_VALUE;
		}
	} else if (strcmp(field_name, "out_dst_ip_type") == 0) {
		result = parse_ip_type(value, &match->out_dst_ip.type);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "out_dst_ip_addr") == 0) {
		if (match->out_dst_ip.type == DOCA_FLOW_IP4_ADDR) {
			result = parse_ipv4_str(value, &match->out_dst_ip.ipv4_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else if (match->out_dst_ip.type == DOCA_FLOW_IP6_ADDR) {
			result = parse_ipv6_str(value, match->out_dst_ip.ipv6_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else {
			DOCA_LOG_ERR("dst ip type is not set, need to set ip type before address");
			return DOCA_ERROR_INVALID_VALUE;
		}
	} else if (strcmp(field_name, "out_l4_type") == 0) {
		result = parse_protocol_string(value, &match->out_l4_type);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "out_tcp_flags") == 0) {
		result = parse_tcp_flag_string(value, &match->out_tcp_flags);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "out_src_port") == 0)
		match->out_src_port = rte_cpu_to_be_16(strtol(value, NULL, 0));

	else if (strcmp(field_name, "out_dst_port") == 0)
		match->out_dst_port = rte_cpu_to_be_16(strtol(value, NULL, 0));

	else if (strcmp(field_name, "tun_type") == 0) {
		result = parse_tun_type_string(value, &match->tun.type);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "vxlan_tun_id") == 0)
		match->tun.vxlan_tun_id = strtol(value, NULL, 0);

	else if (strcmp(field_name, "gre_key") == 0)
		match->tun.gre_key = strtol(value, NULL, 0);

	else if (strcmp(field_name, "gtp_teid") == 0)
		match->tun.gtp_teid = strtol(value, NULL, 0);

	else if (strcmp(field_name, "in_src_mac") == 0) {
		result = parse_mac_address(value, match->in_src_mac);
		if (result != DOCA_SUCCESS)
			return result;
	}

	else if (strcmp(field_name, "in_dst_mac") == 0) {
		result = parse_mac_address(value, match->in_dst_mac);
		if (result != DOCA_SUCCESS)
			return result;
	}

	else if (strcmp(field_name, "in_eth_type") == 0)
		match->in_eth_type = (uint16_t)strtol(value, NULL, HEXADECIMAL_BASE);

	else if (strcmp(field_name, "in_vlan_tci") == 0)
		match->in_vlan_tci = (uint16_t)strtol(value, NULL, 0);

	else if (strcmp(field_name, "in_src_ip_type") == 0) {
		result = parse_ip_type(value, &match->in_src_ip.type);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "in_src_ip_addr") == 0) {
		if (match->in_src_ip.type == DOCA_FLOW_IP4_ADDR) {
			result = parse_ipv4_str(value, &match->in_src_ip.ipv4_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else if (match->in_src_ip.type == DOCA_FLOW_IP6_ADDR) {
			result = parse_ipv6_str(value, match->in_src_ip.ipv6_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else {
			DOCA_LOG_ERR("inner src ip type is not set, need to set ip type before address");
			return DOCA_ERROR_INVALID_VALUE;
		}
	} else if (strcmp(field_name, "in_dst_ip_type") == 0) {
		result = parse_ip_type(value, &match->in_dst_ip.type);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "in_dst_ip_addr") == 0) {
		if (match->in_dst_ip.type == DOCA_FLOW_IP4_ADDR) {
			result = parse_ipv4_str(value, &match->in_dst_ip.ipv4_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else if (match->in_dst_ip.type == DOCA_FLOW_IP6_ADDR) {
			result = parse_ipv6_str(value, match->in_dst_ip.ipv6_addr);
			if (result != DOCA_SUCCESS)
				return result;
		} else {
			DOCA_LOG_ERR("inner dst ip type is not set, need to set ip type before address");
			return DOCA_ERROR_INVALID_VALUE;
		}

	} else if (strcmp(field_name, "in_l4_type") == 0) {
		result = parse_protocol_string(value, &match->in_l4_type);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "in_tcp_flags") == 0) {
		result = parse_tcp_flag_string(value, &match->in_tcp_flags);
		if (result != DOCA_SUCCESS)
			return result;
	} else if (strcmp(field_name, "in_src_port") == 0)
		match->in_src_port = rte_cpu_to_be_16(strtol(value, NULL, 0));

	else if (strcmp(field_name, "in_dst_port") == 0)
		match->in_dst_port = rte_cpu_to_be_16(strtol(value, NULL, 0));

	else {
		DOCA_LOG_ERR("%s is not supported field in match", field_name);
		return DOCA_ERROR_INVALID_VALUE;
	}
	return DOCA_SUCCESS;
}

/*
 * Parse DOCA Flow struct
 *
 * @struct_str [in]: Struct to parse
 * @fill_struct [in]: Function callback to do the actual filling
 * @struct_ptr [out]: Struct to fill
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_struct(char *struct_str, doca_error_t (*fill_struct)(char *, char *, void *), void *struct_ptr)
{
	doca_error_t result;
	char ptr[MAX_CMDLINE_INPUT_LEN];
	char *tmp;
	char field_name[MAX_FIELD_INPUT_LEN];
	char value[MAX_FIELD_INPUT_LEN];
	char tmp_char;

	do {
		strlcpy(ptr, struct_str, MAX_CMDLINE_INPUT_LEN);
		tmp = strtok(ptr, "=");
		if (tmp == NULL) {
			DOCA_LOG_ERR("Invalid format for create struct command");
			return DOCA_ERROR_INVALID_VALUE;
		}
		strlcpy(field_name, tmp, MAX_FIELD_INPUT_LEN);
		struct_str += strlen(field_name) + 1;

		strlcpy(ptr, struct_str, MAX_CMDLINE_INPUT_LEN);
		tmp = strtok(ptr, ",");
		if (tmp == NULL) {
			DOCA_LOG_ERR("Invalid format for create struct command");
			return DOCA_ERROR_INVALID_VALUE;
		}
		strlcpy(value, tmp, MAX_FIELD_INPUT_LEN);

		DOCA_LOG_DBG("field_name: %s,  value: %s", field_name, value);

		struct_str += strlen(value);
		tmp_char = struct_str[0];
		struct_str++;
		result = (*fill_struct)(field_name, value, struct_ptr);
		if (result != DOCA_SUCCESS)
			return result;

	} while (tmp_char == ',');

	return DOCA_SUCCESS;
}

/*
 * Parse boolean user input
 *
 * @params_str [in]: String to parse
 * @param_str_len [in]: Length of boolean value
 * @take_action [out]: Boolean to fill
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_bool_params_input(char **params_str, int param_str_len, bool *take_action)
{
	int value;
	char *ptr;

	*params_str += param_str_len;

	value = strtol(*params_str, &ptr, 0);
	if (ptr == *params_str)
		return DOCA_ERROR_INVALID_VALUE;
	if (value == 1)
		*take_action = true;
	else if (value != 0)
		return DOCA_ERROR_BAD_STATE;

	*params_str += 1;

	return DOCA_SUCCESS;
}

/*
 * Parse create pipe parameters
 *
 * @params_str [in]: String to parse
 * @cfg [out]: DOCA Flow pipe configuration
 * @fwd_action [out]: Boolean to indicate if forward is needed
 * @fwd_miss_action [out]: Boolean to indicate if forward miss is needed
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_create_pipe_params(char *params_str, struct doca_flow_pipe_cfg *cfg, bool *fwd_action, bool *fwd_miss_action)
{
	doca_error_t result;
	char ptr[MAX_CMDLINE_INPUT_LEN];
	char *param_str_value;
	char *type_str;
	char *end;
	int value;
	char tmp_char;
	bool has_port_id = false;
	bool take_action = false;

	do {
		if (strncmp(params_str, "port_id=", PORT_ID_STR_LEN) == 0) {
			params_str += PORT_ID_STR_LEN;
			strlcpy(ptr, params_str, MAX_CMDLINE_INPUT_LEN);
			value = strtol(ptr, &end, 0);
			if (end == ptr) {
				DOCA_LOG_ERR("Invalid port_id");
				return DOCA_ERROR_INVALID_VALUE;
			}
			param_str_value = strtok(ptr, ",");
			params_str += strlen(param_str_value);
			pipe_port_id = value;
			has_port_id = true;
		} else if (strncmp(params_str, "name=", NAME_STR_LEN) == 0) {
			if (cfg->attr.name == NULL) {
				params_str += NAME_STR_LEN;
				strlcpy(ptr, params_str, MAX_CMDLINE_INPUT_LEN);
				param_str_value = strtok(ptr, ",");
				params_str += strlen(param_str_value);
				cfg->attr.name = strndup(param_str_value, MAX_FIELD_INPUT_LEN);
				if (cfg->attr.name == NULL) {
					DOCA_LOG_ERR("failed to allocate memory for name");
					goto error;
				}
			} else
				DOCA_LOG_WARN("Name field is already initialized");
		} else if (strncmp(params_str, "root_enable=", ROOT_ENABLE_STR_LEN) == 0) {
			result = parse_bool_params_input(&params_str, ROOT_ENABLE_STR_LEN, &take_action);
			if (result != DOCA_SUCCESS) {
				DOCA_LOG_ERR("root_enable must be 1 for using or 0 for not");
				goto error;
			}
			if (take_action)
				cfg->attr.is_root = true;
			take_action = false;
		} else if (strncmp(params_str, "monitor=", MONITOR_STR_LEN) == 0) {
			result = parse_bool_params_input(&params_str, MONITOR_STR_LEN, &take_action);
			if (result != DOCA_SUCCESS) {
				DOCA_LOG_ERR("monitor must be 1 for using or 0 for not");
				goto error;
			}
			if (take_action)
				cfg->monitor = &monitor;
			take_action = false;
		} else if (strncmp(params_str, "match_mask=", MATCH_MASK_STR_LEN) == 0) {
			result = parse_bool_params_input(&params_str, MATCH_MASK_STR_LEN, &take_action);
			if (result != DOCA_SUCCESS) {
				DOCA_LOG_ERR("match_mask value must be 1 for using or 0 for not");
				goto error;
			}
			if (take_action)
				cfg->match_mask = &match_mask;
			take_action = false;
		} else if (strncmp(params_str, "fwd_miss=", MISS_FWD_STR_LEN) == 0) {
			result = parse_bool_params_input(&params_str, MISS_FWD_STR_LEN, fwd_miss_action);
			if (result != DOCA_SUCCESS) {
				DOCA_LOG_ERR("fwd_miss value must be 1 for using or 0 for not");
				goto error;
			}
		} else if (strncmp(params_str, "fwd=", FWD_STR_LEN) == 0) {
			result = parse_bool_params_input(&params_str, FWD_STR_LEN, fwd_action);
			if (result != DOCA_SUCCESS) {
				DOCA_LOG_ERR("fwd value must be 1 for using or 0 for not");
				goto error;
			}
		} else if (strncmp(params_str, "type=", TYPE_STR_LEN) == 0) {
			params_str += TYPE_STR_LEN;
			strlcpy(ptr, params_str, MAX_FIELD_INPUT_LEN);
			type_str = strtok(ptr, ",");
			result = parse_pipe_type(type_str, &cfg->attr.type);
			if (result != DOCA_SUCCESS)
				goto error;
			params_str += strlen(type_str);
		} else {
			strlcpy(ptr, params_str, MAX_FIELD_INPUT_LEN);
			param_str_value = strtok(ptr, "=");
			DOCA_LOG_ERR("%s is not a valid parameter for create pipe command", param_str_value);
			goto error;
		}
		tmp_char = params_str[0];
		params_str++;
	} while (tmp_char == ',');

	if (!has_port_id) {
		DOCA_LOG_ERR("port_id is a mandatory input and was not given");
		goto error;
	}
	return DOCA_SUCCESS;
error:
	if (cfg->attr.name != NULL)
		free((void *)cfg->attr.name);
	return DOCA_ERROR_INVALID_VALUE;
}

/*
 * Parse add entry parameters
 *
 * @params_str [in]: String to parse
 * @fwd_action [out]: Boolean to indicate if forward is needed
 * @monitor_action [out]: Boolean to indicate if monitor action is needed
 * @pipe_id [out]: Pipe ID
 * @pipe_queue [out]: Pipe queue number
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_add_entry_params(char *params_str, bool *fwd_action, bool *monitor_action, uint64_t *pipe_id, int *pipe_queue)
{
	doca_error_t result;
	char tmp_char;
	char ptr[MAX_CMDLINE_INPUT_LEN];
	char *param_str_name;
	bool has_pipe_id = false;
	bool has_pipe_queue = false;

	do {
		if (strncmp(params_str, "pipe_id=", PIPE_ID_STR_LEN) == 0) {
			params_str += PIPE_ID_STR_LEN;
			strlcpy(ptr, params_str, MAX_CMDLINE_INPUT_LEN);
			*pipe_id = strtoull(ptr, &params_str, 0);
			has_pipe_id = true;
		} else if (strncmp(params_str, "pipe_queue=", PIPE_QUEUE_STR_LEN) == 0) {
			params_str += PIPE_QUEUE_STR_LEN;
			strlcpy(ptr, params_str, MAX_CMDLINE_INPUT_LEN);
			*pipe_queue = strtol(ptr, &params_str, 0);
			has_pipe_queue = true;
		} else if (strncmp(params_str, "monitor=", MONITOR_STR_LEN) == 0) {
			result = parse_bool_params_input(&params_str, MONITOR_STR_LEN, monitor_action);
			if (result != DOCA_SUCCESS) {
				DOCA_LOG_ERR("fwd value must be 1 for using or 0 for not");
				return DOCA_ERROR_INVALID_VALUE;
			};
		} else if (strncmp(params_str, "fwd=", FWD_STR_LEN) == 0) {
			result = parse_bool_params_input(&params_str, FWD_STR_LEN, fwd_action);
			if (result != DOCA_SUCCESS) {
				DOCA_LOG_ERR("fwd value must be 1 for using or 0 for not");
				return DOCA_ERROR_INVALID_VALUE;
			}
		} else {
			strlcpy(ptr, params_str, MAX_CMDLINE_INPUT_LEN);
			param_str_name = strtok(ptr, "=");
			DOCA_LOG_ERR("%s is not a valid parameter for pipe add entry command", param_str_name);
			return DOCA_ERROR_INVALID_VALUE;
		}
		tmp_char = params_str[0];
		params_str++;
	} while (tmp_char == ',');

	if (!has_pipe_id) {
		DOCA_LOG_ERR("pipe_id is a mandatory input and was not given");
		return -1;
	}

	if (!has_pipe_queue) {
		DOCA_LOG_ERR("pipe_queue is a mandatory input and was not given");
		return DOCA_ERROR_INVALID_VALUE;
	}

	return DOCA_SUCCESS;
}

/*
 * Parse add control pipe entry parameters
 *
 * @params_str [in]: String to parse
 * @fwd_action [out]: Boolean to indicate if forward is needed
 * @match_mask_action [out]: Boolean to indicate if mask action is needed
 * @pipe_id [out]: Pipe ID
 * @pipe_queue [out]: Pipe queue number
 * @priority [out]: DOCA Flow priority
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_add_control_pipe_entry_params(char *params_str, bool *fwd_action, bool *match_mask_action, uint64_t *pipe_id,
				    uint16_t *pipe_queue, uint8_t *priority)
{
	doca_error_t result;
	char tmp_char;
	char ptr[MAX_CMDLINE_INPUT_LEN];
	char *param_str_name;
	bool has_pipe_id = false;
	bool has_pipe_queue = false;
	bool has_priority = false;

	do {
		if (strncmp(params_str, "pipe_id=", PIPE_ID_STR_LEN) == 0) {
			params_str += PIPE_ID_STR_LEN;
			strlcpy(ptr, params_str, MAX_CMDLINE_INPUT_LEN);
			*pipe_id = strtoull(ptr, &params_str, 0);
			has_pipe_id = true;
		} else if (strncmp(params_str, "pipe_queue=", PIPE_QUEUE_STR_LEN) == 0) {
			params_str += PIPE_QUEUE_STR_LEN;
			strlcpy(ptr, params_str, MAX_CMDLINE_INPUT_LEN);
			*pipe_queue = strtol(ptr, &params_str, 0);
			has_pipe_queue = true;
		} else if (strncmp(params_str, "priority=", PRIORITY_STR_LEN) == 0) {
			params_str += PRIORITY_STR_LEN;
			strlcpy(ptr, params_str, MAX_CMDLINE_INPUT_LEN);
			*priority = strtol(ptr, &params_str, 0);
			has_priority = true;
		} else if (strncmp(params_str, "match_mask=", MATCH_MASK_STR_LEN) == 0) {
			result = parse_bool_params_input(&params_str, MATCH_MASK_STR_LEN, match_mask_action);
			if (result != DOCA_SUCCESS) {
				DOCA_LOG_ERR("match_mask value must be 1 for using or 0 for not");
				return DOCA_ERROR_INVALID_VALUE;
			};
		} else if (strncmp(params_str, "fwd=", FWD_STR_LEN) == 0) {
			result = parse_bool_params_input(&params_str, FWD_STR_LEN, fwd_action);
			if (result != DOCA_SUCCESS) {
				DOCA_LOG_ERR("fwd value must be 1 for using or 0 for not");
				return DOCA_ERROR_INVALID_VALUE;
			}
		} else {
			strlcpy(ptr, params_str, MAX_CMDLINE_INPUT_LEN);
			param_str_name = strtok(ptr, "=");
			DOCA_LOG_ERR("%s is not a valid parameter for control pipe add entry command", param_str_name);
			return DOCA_ERROR_INVALID_VALUE;
		}
		tmp_char = params_str[0];
		params_str++;
	} while (tmp_char == ',');

	if (!has_pipe_id) {
		DOCA_LOG_ERR("pipe_id is a mandatory input and was not given");
		return DOCA_ERROR_INVALID_VALUE;
	}

	if (!has_pipe_queue) {
		DOCA_LOG_ERR("pipe_queue is a mandatory input and was not given");
		return DOCA_ERROR_INVALID_VALUE;
	}

	if (!has_priority) {
		DOCA_LOG_ERR("priority is a mandatory input and was not given");
		return DOCA_ERROR_INVALID_VALUE;
	}
	return DOCA_SUCCESS;
}

/*
 * Parse entry parameters
 *
 * @params [in]: String to parse
 * @pipe_queue_mandatory [in]: Boolean to indicate if pipe queue is mandatory
 * @pipe_queue [out]: Pipe queue number
 * @entry_id [out]: Entry ID
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_entry_params(char *params, bool pipe_queue_mandatory, uint16_t *pipe_queue, uint64_t *entry_id)
{
	char tmp_char;
	char ptr[MAX_CMDLINE_INPUT_LEN];
	char *param_str_name;
	bool has_pipe_queue = false;
	bool has_entry_id = false;

	do {
		if (strncmp(params, "pipe_queue=", PIPE_QUEUE_STR_LEN) == 0) {
			params += PIPE_QUEUE_STR_LEN;
			strlcpy(ptr, params, MAX_CMDLINE_INPUT_LEN);
			if (pipe_queue != NULL)
				*pipe_queue = strtol(ptr, &params, 0);
			has_pipe_queue = true;
		} else if (strncmp(params, "entry_id=", ENTRY_ID_STR_LEN) == 0) {
			params += ENTRY_ID_STR_LEN;
			strlcpy(ptr, params, MAX_CMDLINE_INPUT_LEN);
			*entry_id = strtoull(ptr, &params, 0);
			has_entry_id = true;
		} else {
			strlcpy(ptr, params, MAX_CMDLINE_INPUT_LEN);
			param_str_name = strtok(ptr, "=");
			DOCA_LOG_ERR("%s is not a valid parameter for rm/query entry command", param_str_name);
			return DOCA_ERROR_INVALID_VALUE;
		}
		tmp_char = params[0];
		params++;
	} while (tmp_char == ',');

	if (pipe_queue_mandatory && !has_pipe_queue) {
		DOCA_LOG_ERR("pipe_queue is a mandatory input and was not given");
		return DOCA_ERROR_INVALID_VALUE;
	}

	if (!has_entry_id) {
		DOCA_LOG_ERR("entry_id is a mandatory input and was not given");
		return DOCA_ERROR_INVALID_VALUE;
	}
	return DOCA_SUCCESS;
}

/*
 * Parse dump pipe parameters
 *
 * @params [in]: String to parse
 * @port_id [out]: Port ID
 * @file [out]: File to dump information into
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
parse_dump_pipe_params(char *params, uint16_t *port_id, FILE **file)
{
	char tmp_char;
	char *name;
	char ptr[MAX_CMDLINE_INPUT_LEN];
	char *param_str_name;
	bool has_port_id = false;
	bool has_file = false;

	do {
		if (strncmp(params, "port_id=", PORT_ID_STR_LEN) == 0) {
			params += PORT_ID_STR_LEN;
			strlcpy(ptr, params, MAX_CMDLINE_INPUT_LEN);
			*port_id = strtol(ptr, &params, 0);
			has_port_id = true;
		} else if (strncmp(params, "file=", FILE_STR_LEN) == 0) {
			if (has_file) {
				DOCA_LOG_ERR("file should be provided only once");
				fclose(*file);
				return DOCA_ERROR_INVALID_VALUE;
			}
			params += FILE_STR_LEN;
			strlcpy(ptr, params, MAX_CMDLINE_INPUT_LEN);
			name = strtok(ptr, ",");
			params += strlen(name);
			*file = fopen(name, "w");
			if (*file == NULL) {
				DOCA_LOG_ERR("failed opening the file %s", name);
				return DOCA_ERROR_INVALID_VALUE;
			}
			has_file = true;
		} else {
			strlcpy(ptr, params, MAX_CMDLINE_INPUT_LEN);
			param_str_name = strtok(ptr, "=");
			DOCA_LOG_ERR("%s is not a valid parameter for port pipes dump command", param_str_name);
			return DOCA_ERROR_INVALID_VALUE;
		}
		tmp_char = params[0];
		params++;
	} while (tmp_char == ',');

	if (!has_port_id) {
		DOCA_LOG_ERR("port_id is a mandatory input and was not given");
		return DOCA_ERROR_INVALID_VALUE;
	}

	if (!has_file) {
		DOCA_LOG_DBG("file name was not given, default name is port_info.txt");
		*file = fopen("port_info.txt", "w");
		if (*file == NULL) {
			DOCA_LOG_ERR("failed opening the file port_info.txt");
			return DOCA_ERROR_IO_FAILED;
		}
	}
	return DOCA_SUCCESS;
}

/*
 * Parse create pipe command and call command's callback
 *
 * @parsed_result [in]: Command line interface input with user input
 */
static void
cmd_create_pipe_parsed(void *parsed_result, __rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_create_pipe_result *create_pipe_data = (struct cmd_create_pipe_result *)parsed_result;
	struct doca_flow_fwd *tmp_fwd = NULL;
	struct doca_flow_fwd *tmp_fwd_miss = NULL;
	struct doca_flow_pipe_cfg pipe_cfg = {.match = &pipe_match};
	struct doca_flow_actions *actions_arr[1];
	bool is_fwd = false;
	bool is_fwd_miss = false;
	doca_error_t result;

	if (create_pipe_func == NULL) {
		DOCA_LOG_ERR("Pipe creation action was not inserted");
		return;
	}

	actions_arr[0] = &actions;
	pipe_cfg.actions = actions_arr;
	pipe_cfg.attr.nb_actions = 1;
	result = parse_create_pipe_params(create_pipe_data->params, &pipe_cfg, &is_fwd, &is_fwd_miss);
	if (result != DOCA_SUCCESS)
		return;

	if (is_fwd)
		tmp_fwd = &fwd;

	if (is_fwd_miss)
		tmp_fwd_miss = &fwd_miss;

	(*create_pipe_func)(&pipe_cfg, pipe_port_id, tmp_fwd, fwd_next_pipe_id, tmp_fwd_miss, fwd_miss_next_pipe_id);
}

/* Define the token of create */
cmdline_parse_token_string_t cmd_create_pipe_create_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_create_pipe_result, create, "create");

/* Define the token of pipe */
cmdline_parse_token_string_t cmd_create_pipe_pipe_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_create_pipe_result, pipe, "pipe");

/* Define the token of optional params */
cmdline_parse_token_string_t cmd_create_pipe_optional_fields_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_create_pipe_result, params, NULL);

/* Define create pipe command structure for parsing */
cmdline_parse_inst_t cmd_create_pipe = {
	.f = cmd_create_pipe_parsed,					/* Function to call */
	.data = NULL,							/* 2nd arg of func */
	.help_str = "create pipe port_id=[port_id],[optional params]",	/* Command print usage */
	.tokens = {							/* Token list, NULL terminated */
			(void *)&cmd_create_pipe_create_tok,
			(void *)&cmd_create_pipe_pipe_tok,
			(void *)&cmd_create_pipe_optional_fields_tok,
			NULL,
		},
};

/*
 * Parse add entry command and call command's callback
 *
 * @parsed_result [in]: Command line interface input with user input
 */
static void
cmd_add_entry_parsed(void *parsed_result, __rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_add_entry_result *add_entry_data = (struct cmd_add_entry_result *)parsed_result;
	struct doca_flow_fwd *tmp_fwd = NULL;
	struct doca_flow_monitor *tmp_monitor = NULL;
	bool is_fwd = false;
	bool is_monitor = false;
	uint64_t pipe_id = 0;
	int pipe_queue = 0;
	doca_error_t result;

	if (add_entry_func == NULL) {
		DOCA_LOG_ERR("Entry creation action was not inserted");
		return;
	}

	result = parse_add_entry_params(add_entry_data->params, &is_fwd, &is_monitor, &pipe_id, &pipe_queue);
	if (result != DOCA_SUCCESS)
		return;

	if (is_fwd)
		tmp_fwd = &fwd;

	if (is_monitor)
		tmp_monitor = &monitor;

	(*add_entry_func)(pipe_queue, pipe_id, &entry_match, &actions, tmp_monitor, tmp_fwd, fwd_next_pipe_id,
			  DOCA_FLOW_NO_WAIT);
}

/* Define the token of add */
cmdline_parse_token_string_t cmd_add_entry_add_tok = TOKEN_STRING_INITIALIZER(struct cmd_add_entry_result, add, "add");

/* Define the token of entry */
cmdline_parse_token_string_t cmd_add_entry_entry_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_add_entry_result, entry, "entry");

/* Define the token of optional params */
cmdline_parse_token_string_t cmd_add_entry_optional_fields_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_add_entry_result, params, NULL);

/* Define add entry command structure for parsing */
cmdline_parse_inst_t cmd_add_entry = {
	.f = cmd_add_entry_parsed,								/* Function to call */
	.data = NULL,										/* 2nd arg of func */
	.help_str = "add entry pipe_id=[pipe_id],pipe_queue=[pipe_queue],[optional fields]",	/* Command print usage */
	.tokens = {										/* Token list, NULL terminated */
			(void *)&cmd_add_entry_add_tok,
			(void *)&cmd_add_entry_entry_tok,
			(void *)&cmd_add_entry_optional_fields_tok,
			NULL,
		},
};

/*
 * Parse add control pipe entry command and call command's callback
 *
 * @parsed_result [in]: Command line interface input with user input
 */
static void
cmd_add_control_pipe_entry_parsed(void *parsed_result, __rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_add_control_pipe_entry_result *add_entry_data =
		(struct cmd_add_control_pipe_entry_result *)parsed_result;
	struct doca_flow_fwd *tmp_fwd = NULL;
	struct doca_flow_match *tmp_match_mask = NULL;
	bool is_fwd = false;
	bool is_match_mask = false;
	uint64_t pipe_id = 0;
	uint16_t pipe_queue = 0;
	uint8_t priority = 0;
	doca_error_t result;

	if (add_control_pipe_entry_func == NULL) {
		DOCA_LOG_ERR("Control pipe entry creation action was not inserted");
		return;
	}

	result = parse_add_control_pipe_entry_params(add_entry_data->params, &is_fwd, &is_match_mask, &pipe_id,
						  &pipe_queue, &priority);
	if (result != DOCA_SUCCESS)
		return;

	if (is_fwd)
		tmp_fwd = &fwd;
	if (is_match_mask)
		tmp_match_mask = &match_mask;

	(*add_control_pipe_entry_func)(pipe_queue, priority, pipe_id, &entry_match, tmp_match_mask, tmp_fwd,
				       fwd_next_pipe_id);
}

/* Define the token of add */
cmdline_parse_token_string_t cmd_add_control_pipe_entry_add_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_add_control_pipe_entry_result, add, "add");

/* Define the token of control_pipe */
cmdline_parse_token_string_t cmd_add_control_pipe_entry_control_pipe_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_add_control_pipe_entry_result, control_pipe, "control_pipe");

/* Define the token of entry */
cmdline_parse_token_string_t cmd_add_control_pipe_entry_entry_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_add_control_pipe_entry_result, entry, "entry");

/* Define the token of optional params */
cmdline_parse_token_string_t cmd_add_control_pipe_entry_optional_fields_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_add_control_pipe_entry_result, params, NULL);

/* Define add control pipe entry command structure for parsing */
cmdline_parse_inst_t cmd_add_control_pipe_entry = {
	.f = cmd_add_control_pipe_entry_parsed,					/* Function to call */
	.data = NULL,								/* 2nd arg of func */
	.help_str =								/* Command print usage */
		"add control_pipe entry priority=[priority],port_id=[port_id],pipe_id=[pipe_id],pipe_queue=[pipe_queue],[optional fields]",
	.tokens = {								/* Token list, NULL terminated */
			(void *)&cmd_add_control_pipe_entry_add_tok,
			(void *)&cmd_add_control_pipe_entry_control_pipe_tok,
			(void *)&cmd_add_control_pipe_entry_entry_tok,
			(void *)&cmd_add_control_pipe_entry_optional_fields_tok,
			NULL,
		},
};

/*
 * Parse destroy pipe command and call command's callback
 *
 * @parsed_result [in]: Command line interface input with user input
 */
static void
cmd_destroy_pipe_parsed(void *parsed_result, __rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_destroy_pipe_result *destroy_pipe_data = (struct cmd_destroy_pipe_result *)parsed_result;
	uint64_t pipe_id;
	doca_error_t result;

	if (destroy_pipe_func == NULL) {
		DOCA_LOG_ERR("Pipe destruction action was not inserted");
		return;
	}

	result = parse_pipe_id_input(destroy_pipe_data->params, &pipe_id);
	if (result != DOCA_SUCCESS)
		return;

	(*destroy_pipe_func)(pipe_id);
}

/* Define the token of destroy */
cmdline_parse_token_string_t cmd_destroy_pipe_destroy_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_destroy_pipe_result, destroy, "destroy");

/* Define the token of pipe */
cmdline_parse_token_string_t cmd_destroy_pipe_pipe_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_destroy_pipe_result, pipe, "pipe");

/* Define the token of optional params */
cmdline_parse_token_string_t cmd_destroy_pipe_params_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_destroy_pipe_result, params, NULL);

/* Define destroy pipe command structure for parsing */
cmdline_parse_inst_t cmd_destroy_pipe = {
	.f = cmd_destroy_pipe_parsed,					/* Function to call */
	.data = NULL,							/* 2nd arg of func */
	.help_str = "destroy pipe pipe_id=[pipe_id]",			/* Command print usage */
	.tokens = {							/* Token list, NULL terminated */
			(void *)&cmd_destroy_pipe_destroy_tok,
			(void *)&cmd_destroy_pipe_pipe_tok,
			(void *)&cmd_destroy_pipe_params_tok,
			NULL,
		},
};

/*
 * Parse remove entry command and call command's callback
 *
 * @parsed_result [in]: Command line interface input with user input
 */
static void
cmd_rm_entry_parsed(void *parsed_result, __rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_rm_entry_result *rm_entry_data = (struct cmd_rm_entry_result *)parsed_result;
	uint64_t entry_id = 0;
	uint16_t pipe_queue = 0;
	doca_error_t result;

	if (remove_entry_func == NULL) {
		DOCA_LOG_ERR("Entry destruction action was not inserted");
		return;
	}

	result = parse_entry_params(rm_entry_data->params, true, &pipe_queue, &entry_id);
	if (result != DOCA_SUCCESS)
		return;

	(*remove_entry_func)(pipe_queue, entry_id);
}

/* Define the token of remove */
cmdline_parse_token_string_t cmd_rm_entry_rm_tok = TOKEN_STRING_INITIALIZER(struct cmd_rm_entry_result, rm, "rm");

/* Define the token of entry */
cmdline_parse_token_string_t cmd_rm_entry_entry_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_rm_entry_result, entry, "entry");

/* Define the token of optional params */
cmdline_parse_token_string_t cmd_rm_entry_params_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_rm_entry_result, params, NULL);

/* Define remove entry command structure for parsing */
cmdline_parse_inst_t cmd_rm_entry = {
	.f = cmd_rm_entry_parsed,						/* Function to call */
	.data = NULL,								/* 2nd arg of func */
	.help_str = "rm entry pipe_queue=[pipe_queue],entry_id=[entry_id]",	/* Command print usage */
	.tokens = {								/* Token list, NULL terminated */
			(void *)&cmd_rm_entry_rm_tok,
			(void *)&cmd_rm_entry_entry_tok,
			(void *)&cmd_rm_entry_params_tok,
			NULL,
		},
};

/*
 * Parse flush pipes command and call command's callback
 *
 * @parsed_result [in]: Command line interface input with user input
 */
static void
cmd_flush_pipes_parsed(void *parsed_result, __rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_flush_pipes_result *flush_pipes_data = (struct cmd_flush_pipes_result *)parsed_result;
	int port_id;
	doca_error_t result;

	if (port_pipes_flush_func == NULL) {
		DOCA_LOG_ERR("Pipes flushing action was not inserted");
		return;
	}

	result = parse_port_id_input(flush_pipes_data->port_id, &port_id);
	if (result != DOCA_SUCCESS)
		return;

	(*port_pipes_flush_func)(port_id);
}

/* Define the token of port */
cmdline_parse_token_string_t cmd_flush_pipes_port_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_flush_pipes_result, port, "port");

/* Define the token of pipes */
cmdline_parse_token_string_t cmd_flush_pipes_pipes_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_flush_pipes_result, pipes, "pipes");

/* Define the token of flush */
cmdline_parse_token_string_t cmd_flush_pipes_flush_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_flush_pipes_result, flush, "flush");

/* Define the token of optional params */
cmdline_parse_token_string_t cmd_flush_pipes_port_id_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_flush_pipes_result, port_id, NULL);

/* Define flush pipes command structure for parsing */
cmdline_parse_inst_t cmd_flush_pipe = {
	.f = cmd_flush_pipes_parsed,				/* Function to call */
	.data = NULL,						/* 2nd arg of func */
	.help_str = "port pipes flush port_id=[port_id]",	/* Command print usage */
	.tokens = {						/* Token list, NULL terminated */
			(void *)&cmd_flush_pipes_port_tok,
			(void *)&cmd_flush_pipes_pipes_tok,
			(void *)&cmd_flush_pipes_flush_tok,
			(void *)&cmd_flush_pipes_port_id_tok,
			NULL,
		},
};

/*
 * Parse query command and call command's callback
 *
 * @parsed_result [in]: Command line interface input with user input
 */
static void
cmd_query_parsed(void *parsed_result, __rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_query_result *query_data = (struct cmd_query_result *)parsed_result;
	struct doca_flow_query query_stats;
	uint64_t entry_id = 0;
	doca_error_t result;

	if (query_func == NULL) {
		DOCA_LOG_ERR("Query action was not inserted");
		return;
	}

	result = parse_entry_params(query_data->params, false, NULL, &entry_id);
	if (result != DOCA_SUCCESS)
		return;

	(*query_func)(entry_id, &query_stats);

	DOCA_LOG_INFO("Total bytes: %ld", query_stats.total_bytes);
	DOCA_LOG_INFO("Total packets: %ld", query_stats.total_pkts);
}

/* Define the token of query */
cmdline_parse_token_string_t cmd_query_query_tok = TOKEN_STRING_INITIALIZER(struct cmd_query_result, query, "query");

/* Define the token of optional params */
cmdline_parse_token_string_t cmd_query_params_tok = TOKEN_STRING_INITIALIZER(struct cmd_query_result, params, NULL);

/* Define entry query command structure for parsing */
cmdline_parse_inst_t cmd_query = {
	.f = cmd_query_parsed,				/* Function to call */
	.data = NULL,					/* 2nd arg of func */
	.help_str = "query entry_id=[entry_id]",	/* Command print usage */
	.tokens = {					/* Token list, NULL terminated */
			(void *)&cmd_query_query_tok,
			(void *)&cmd_query_params_tok,
			NULL,
		},
};

/*
 * Parse dump pipe command and call command's callback
 *
 * @parsed_result [in]: Command line interface input with user input
 */
static void
cmd_dump_pipe_parsed(void *parsed_result, __rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_dump_pipe_result *dump_pipe_data = (struct cmd_dump_pipe_result *)parsed_result;
	uint16_t port_id = 0;
	FILE *fd = NULL;
	doca_error_t result;

	if (port_pipes_dump_func == NULL) {
		DOCA_LOG_ERR("Pipe dumping action was not inserted");
		return;
	}

	result = parse_dump_pipe_params(dump_pipe_data->params, &port_id, &fd);
	if (result != DOCA_SUCCESS) {
		if (fd)
			fclose(fd);
		return;
	}

	(*port_pipes_dump_func)(port_id, fd);

	fclose(fd);
}

/* Define the token of port */
cmdline_parse_token_string_t cmd_port_pipes_dump_port_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_dump_pipe_result, port, "port");

/* Define the token of pipes */
cmdline_parse_token_string_t cmd_port_pipes_dump_pipes_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_dump_pipe_result, pipes, "pipes");

/* Define the token of dump */
cmdline_parse_token_string_t cmd_port_pipes_dump_dump_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_dump_pipe_result, dump, "dump");

/* Define the token of optional params */
cmdline_parse_token_string_t cmd_dump_pipe_params_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_dump_pipe_result, params, NULL);

/* Define dump pipe command structure for parsing */
cmdline_parse_inst_t cmd_dump_pipe = {
	.f = cmd_dump_pipe_parsed,						/* Function to call */
	.data = NULL,								/* 2nd arg of func */
	.help_str = "port pipes dump port_id=[port_id],file=[file name]",	/* Command print usage */
	.tokens = {								/* Token list, NULL terminated */
			(void *)&cmd_port_pipes_dump_port_tok,
			(void *)&cmd_port_pipes_dump_pipes_tok,
			(void *)&cmd_port_pipes_dump_dump_tok,
			(void *)&cmd_dump_pipe_params_tok,
			NULL,
		},
};

/*
 * Parse create DOCA Flow struct command
 *
 * @parsed_result [in]: Command line interface input with user input
 */
static void
cmd_create_struct_parsed(void *parsed_result, __rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_create_struct_result *struct_data = (struct cmd_create_struct_result *)parsed_result;

	if (strcmp(struct_data->flow_struct, "pipe_match") == 0) {
		memset(&pipe_match, 0, sizeof(pipe_match));
		parse_struct(struct_data->flow_struct_input, &parse_match_field, (void *)&pipe_match);
	} else if (strcmp(struct_data->flow_struct, "entry_match") == 0) {
		memset(&entry_match, 0, sizeof(entry_match));
		parse_struct(struct_data->flow_struct_input, &parse_match_field, (void *)&entry_match);
	} else if (strcmp(struct_data->flow_struct, "match_mask") == 0) {
		memset(&match_mask, 0, sizeof(match_mask));
		parse_struct(struct_data->flow_struct_input, &parse_match_field, (void *)&match_mask);
	} else if (strcmp(struct_data->flow_struct, "actions") == 0) {
		memset(&actions, 0, sizeof(actions));
		parse_struct(struct_data->flow_struct_input, &parse_actions_field, (void *)&actions);
	} else if (strcmp(struct_data->flow_struct, "monitor") == 0) {
		memset(&monitor, 0, sizeof(monitor));
		parse_struct(struct_data->flow_struct_input, &parse_monitor_field, (void *)&monitor);
	} else if (strcmp(struct_data->flow_struct, "fwd") == 0) {
		memset(&fwd, 0, sizeof(fwd));
		parse_struct(struct_data->flow_struct_input, &parse_fwd_field, (void *)&fwd);
	} else if (strcmp(struct_data->flow_struct, "fwd_miss") == 0) {
		memset(&fwd_miss, 0, sizeof(fwd_miss));
		parse_struct(struct_data->flow_struct_input, &parse_fwd_miss_field, (void *)&fwd_miss);
	}
}

/* Define the token of create */
cmdline_parse_token_string_t cmd_create_struct_update_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_create_struct_result, create, "create");

/* Define the token of flow type */
cmdline_parse_token_string_t cmd_create_struct_struct_tok = TOKEN_STRING_INITIALIZER(
	struct cmd_create_struct_result, flow_struct, "pipe_match#entry_match#match_mask#actions#monitor#fwd#fwd_miss");

/* Define the token of flow structure */
cmdline_parse_token_string_t cmd_create_struct_input_tok =
	TOKEN_STRING_INITIALIZER(struct cmd_create_struct_result, flow_struct_input, TOKEN_STRING_MULTI);

cmdline_parse_inst_t cmd_update_struct = {
	.f = cmd_create_struct_parsed,					/* Function to call */
	.data = NULL,							/* 2nd arg of func */
	.help_str =							/* Command print usage */
			"create pipe_match|entry_match|match_mask|actions|monitor|fwd|fwd_miss <struct fields>",
	.tokens = {							/* Token list, NULL terminated */
			(void *)&cmd_create_struct_update_tok,
			(void *)&cmd_create_struct_struct_tok,
			(void *)&cmd_create_struct_input_tok,
			NULL,
		},
};

/*
 * Quit command line interface
 *
 * @cl [in]: Command line
 */
static void
cmd_quit_parsed(__rte_unused void *parsed_result, struct cmdline *cl, __rte_unused void *data)
{
	cmdline_quit(cl);
}

/* Define the token of quit */
cmdline_parse_token_string_t cmd_quit_tok = TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit, "quit");

/* Define quit command structure for parsing */
cmdline_parse_inst_t cmd_quit = {
	.f = cmd_quit_parsed,			/* Function to call */
	.data = NULL,				/* 2nd arg of func */
	.help_str = "Exit application",		/* Command print usage */
	.tokens = {				/* Token list, NULL terminated */
			(void *)&cmd_quit_tok,
			NULL,
		},
};

/* Command line interface context */
cmdline_parse_ctx_t main_ctx[] = {
	(cmdline_parse_inst_t *)&cmd_quit,
	(cmdline_parse_inst_t *)&cmd_update_struct,
	(cmdline_parse_inst_t *)&cmd_create_pipe,
	(cmdline_parse_inst_t *)&cmd_add_entry,
	(cmdline_parse_inst_t *)&cmd_add_control_pipe_entry,
	(cmdline_parse_inst_t *)&cmd_destroy_pipe,
	(cmdline_parse_inst_t *)&cmd_rm_entry,
	(cmdline_parse_inst_t *)&cmd_flush_pipe,
	(cmdline_parse_inst_t *)&cmd_dump_pipe,
	(cmdline_parse_inst_t *)&cmd_query,
	NULL,
};

doca_error_t
flow_parser_init(char *shell_prompt)
{
	struct cmdline *cl;

	reset_doca_flow_structs();

	cl = cmdline_stdin_new(main_ctx, shell_prompt);
	if (cl == NULL)
		return DOCA_ERROR_INVALID_VALUE;
	cmdline_interact(cl);
	cmdline_stdin_exit(cl);

	return DOCA_SUCCESS;
}

void
flow_parser_cleanup()
{
	if (rss_queues)
		free(rss_queues);
}
