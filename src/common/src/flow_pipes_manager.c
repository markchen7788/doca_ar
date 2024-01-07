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

#include <rte_errno.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_malloc.h>

#include <doca_log.h>
#include <doca_error.h>

#include "flow_pipes_manager.h"

#define FNV_OFFSET 0xcbf29ce484222325	/* FNV-1a offset basis */
#define FNV_PRIME 0x100000001b3		/* FNV-1a prime */
#define DEFAULT_ENTRIES_NUM 128		/* entry table init size */

DOCA_LOG_REGISTER(FLOW_PIPES_MANAGER);

/*
 * FNV-1a hash algorithm for pointer to id mapping
 *
 * @ptr [in]: Flow to send
 * @return: Pointer to the allocated ID
 */
static uint64_t*
generateID(void *ptr)
{
	uint64_t id = FNV_OFFSET;
	const unsigned char *byte = (const unsigned char *)ptr;
	uint64_t *id_ptr;
	uint8_t bytes_num = sizeof(ptr);

	id_ptr = (uint64_t *)rte_malloc(NULL, sizeof(uint64_t), 0);

	while (bytes_num--)
		id = (uint64_t)((*byte++) ^ id) * FNV_PRIME;

	*id_ptr = id;
	return id_ptr;
}

/*
 * Create pipe information structure
 *
 * @pipe [in]: DOCA Flow pipe pointer
 * @pipes_table [in]: Pipes table of the relevant port on which the pipe was created
 * @return: Pointer to the allocated pipe info structure
 */
static struct pipe_info *
create_pipe_info(struct doca_flow_pipe *pipe, struct rte_hash *pipes_table)
{
	struct pipe_info *pipe_info;
	struct rte_hash_parameters table_params = {
		.name = "Entries Table",
		.entries = DEFAULT_ENTRIES_NUM,
		.key_len = sizeof(uint64_t),
		.hash_func = rte_jhash,
		.hash_func_init_val = 0
	};

	pipe_info = (struct pipe_info *)rte_malloc(NULL, sizeof(struct pipe_info), 0);
	if (pipe_info == NULL) {
		DOCA_LOG_ERR("Failed to allocate memory for Flow Pipes Manager pipe_info structure");
		return NULL;
	}

	pipe_info->pipe = pipe;
	pipe_info->port_id_to_pipes_table = pipes_table;
	pipe_info->entries_table = rte_hash_create(&table_params);
	if (pipe_info->entries_table == NULL) {
		rte_free(pipe_info);
		DOCA_LOG_ERR("Failed to allocate memory for Flow Pipes Manager entries");
		return NULL;
	}

	return pipe_info;
}

/*
 * Destroy pipe information structure
 *
 * @pipe_info [in]: Pipe information structure to destroy
 */
static void
destroy_pipe_info(struct pipe_info *pipe_info)
{
	rte_hash_free(pipe_info->entries_table);
	rte_free(pipe_info);
}

doca_error_t
create_pipes_manager(struct flow_pipes_manager **pipes_manager)
{
	struct rte_hash_parameters table_params = {
		.name = "Pipe id to Pipe info Entries",
		.entries = DEFAULT_ENTRIES_NUM,
		.key_len = sizeof(uint64_t),
		.hash_func = rte_jhash,
		.hash_func_init_val = 0
	};

	*pipes_manager = (struct flow_pipes_manager *)rte_malloc(NULL, sizeof(struct flow_pipes_manager), 0);
	if (*pipes_manager == NULL) {
		DOCA_LOG_ERR("Failed to allocate memory for Flow Pipes Manager - %s", rte_strerror(rte_errno));
		return DOCA_ERROR_NO_MEMORY;
	}

	(*pipes_manager)->pipe_id_to_pipe_info_table = rte_hash_create(&table_params);
	if ((*pipes_manager)->pipe_id_to_pipe_info_table == NULL) {
		rte_free(*pipes_manager);
		DOCA_LOG_ERR("Failed to allocate memory for Flow Pipes Manager table - %s", rte_strerror(rte_errno));
		return DOCA_ERROR_DRIVER;
	}

	table_params.name = "Entry id to Pipe id Table";

	(*pipes_manager)->entry_id_to_pipe_id_table = rte_hash_create(&table_params);
	if ((*pipes_manager)->entry_id_to_pipe_id_table == NULL) {
		rte_free(*pipes_manager);
		rte_hash_free((*pipes_manager)->pipe_id_to_pipe_info_table);
		DOCA_LOG_ERR("Failed to allocate memory for Flow Pipes Manager table -  %s", rte_strerror(rte_errno));
		return DOCA_ERROR_DRIVER;
	}

	table_params.name = "Port id to Pipe Entries Table";
	table_params.key_len = sizeof(uint16_t);

	(*pipes_manager)->port_id_to_pipes_id_table = rte_hash_create(&table_params);
	if ((*pipes_manager)->port_id_to_pipes_id_table == NULL) {
		rte_free(*pipes_manager);
		rte_hash_free((*pipes_manager)->pipe_id_to_pipe_info_table);
		rte_hash_free((*pipes_manager)->entry_id_to_pipe_id_table);
		DOCA_LOG_ERR("Failed to allocate memory for Flow Pipes Manager table - %s", rte_strerror(rte_errno));
		return DOCA_ERROR_DRIVER;
	}

	return DOCA_SUCCESS;
}

void
destroy_pipes_manager(struct flow_pipes_manager *manager)
{
	struct rte_hash *pipes_table;
	struct doca_flow_pipe_entry *entry;
	struct pipe_info *pipe_info;
	uint64_t *pipe_id, *generated_entry_id;
	uint32_t pipe_itr = 0, entry_itr;
	uint16_t *port_id;

	while (rte_hash_iterate(manager->pipe_id_to_pipe_info_table, (const void **)&pipe_id, (void **)&pipe_info, &pipe_itr) >= 0) {
		entry_itr = 0;
		while (rte_hash_iterate(pipe_info->entries_table, (const void **)&generated_entry_id, (void **)&entry, &entry_itr) >= 0)
			rte_free(generated_entry_id);

		rte_free(pipe_id);
		destroy_pipe_info(pipe_info);
	}

	pipe_itr = 0;
	while (rte_hash_iterate(manager->port_id_to_pipes_id_table, (const void **)&port_id, (void **)&pipes_table, &pipe_itr) >= 0)
		rte_hash_free(pipes_table);

	rte_hash_free(manager->port_id_to_pipes_id_table);
	rte_hash_free(manager->entry_id_to_pipe_id_table);
	rte_hash_free(manager->pipe_id_to_pipe_info_table);
	rte_free(manager);
}

doca_error_t
pipes_manager_pipe_create(struct flow_pipes_manager *manager, struct doca_flow_pipe *pipe, uint16_t port_id, uint64_t *pipe_id)
{
	struct pipe_info *pipe_info;
	struct rte_hash *pipes_table;
	struct rte_hash_parameters table_params = {
		.name = "Pipe id's only",
		.entries = DEFAULT_ENTRIES_NUM,
		.key_len = sizeof(uint64_t),
		.hash_func = rte_jhash,
		.hash_func_init_val = 0
	};
	uint64_t *generated_pipe_id;
	uint16_t *port_id_key;
	int32_t result;
	bool is_new_table = false;

	generated_pipe_id = generateID(pipe);
	if (rte_hash_lookup(manager->pipe_id_to_pipe_info_table, (const void *)generated_pipe_id) >= 0) {
		DOCA_LOG_ERR("Could not add new pipe with id=%" PRIu64", id already exists", *generated_pipe_id);
		return DOCA_ERROR_INVALID_VALUE;
	}

	result = rte_hash_lookup_data(manager->port_id_to_pipes_id_table, (const void *)&port_id, (void **)&pipes_table);
	if (result < 0) {
		is_new_table = true;

		/* allocate new port id key */
		port_id_key = (uint16_t *)rte_malloc(NULL, sizeof(uint16_t), 0);
		*port_id_key = port_id;

		pipes_table = rte_hash_create(&table_params);
		if (pipes_table == NULL) {
			DOCA_LOG_ERR("Could not create new pipes table for pipe with id=%" PRIu64"", *generated_pipe_id);
			rte_free(port_id_key);
			rte_free(generated_pipe_id);
			return DOCA_ERROR_NO_MEMORY;
		}

		result = rte_hash_add_key(pipes_table, (const void *)generated_pipe_id);
		if (result < 0) {
			DOCA_LOG_ERR("Could not add new pipe with id=%" PRIu64", to relevant pipes table", *generated_pipe_id);
			rte_hash_free(pipes_table);
			rte_free(port_id_key);
			rte_free(generated_pipe_id);
			return DOCA_ERROR_INVALID_VALUE;
		}

		result = rte_hash_add_key_data(manager->port_id_to_pipes_id_table, (const void *)port_id_key, (void *)pipes_table);
		if (result != 0) {
			DOCA_LOG_ERR("Could not add new pipes table to port to pipes table");
			rte_hash_del_key(pipes_table, (const void *)generated_pipe_id);
			rte_hash_free(pipes_table);
			rte_free(port_id_key);
			return DOCA_ERROR_INVALID_VALUE;
		}

	} else {
		result = rte_hash_add_key(pipes_table, (const void *)generated_pipe_id);
		if (result < 0) {
			DOCA_LOG_ERR("Could not add new pipe id=%" PRIu64" to relevant pipes table", *generated_pipe_id);
			rte_free(generated_pipe_id);
			return DOCA_ERROR_INVALID_VALUE;
		}
	}

	pipe_info = create_pipe_info(pipe, pipes_table);
	if (pipe_info == NULL) {
		DOCA_LOG_ERR("Could not add new pipe id=%" PRIu64" to relevant pipe_info", *generated_pipe_id);
		rte_hash_del_key(pipes_table, (const void *)generated_pipe_id);
		if (is_new_table) {
			rte_hash_del_key(manager->port_id_to_pipes_id_table, (const void *)port_id_key);
			rte_hash_free(pipes_table);
		}
		return DOCA_ERROR_NO_MEMORY;
	}

	result = rte_hash_add_key_data(manager->pipe_id_to_pipe_info_table, (const void *)generated_pipe_id, (void *)pipe_info);
	if (result != 0) {
		DOCA_LOG_ERR("Could not add new pipe_info with pipe id=%" PRIu64" to relevant pipe info table", *generated_pipe_id);
		rte_hash_del_key(pipes_table, (const void *)generated_pipe_id);
		destroy_pipe_info(pipe_info);
		if (is_new_table) {
			rte_hash_del_key(manager->port_id_to_pipes_id_table, (const void *)port_id_key);
			rte_hash_free(pipes_table);
		}
		return DOCA_ERROR_INVALID_VALUE;
	}

	*pipe_id = *generated_pipe_id;

	return DOCA_SUCCESS;
}

doca_error_t
pipes_manager_pipe_add_entry(struct flow_pipes_manager *manager, struct doca_flow_pipe_entry *entry,
					uint64_t pipe_id, uint64_t *entry_id)
{
	uint64_t *generated_entry_id, *pipe_id_ptr;
	struct pipe_info *pipe_info;
	struct rte_hash *entries_table;
	int result;

	generated_entry_id = generateID(entry);

	result = rte_hash_lookup_data(manager->pipe_id_to_pipe_info_table, (const void *)&pipe_id, (void **)&pipe_info);
	if (result < 0) {
		DOCA_LOG_ERR("Could not find relevant pipe id, entry was not entered");
		return DOCA_ERROR_INVALID_VALUE;
	}

	if (rte_hash_get_key_with_position(manager->pipe_id_to_pipe_info_table, result, (void **)&pipe_id_ptr) != 0) {
		DOCA_LOG_ERR("Could not find relevant pipe id, entry was not entered");
		return DOCA_ERROR_NOT_FOUND;
	}

	entries_table = pipe_info->entries_table;

	if (rte_hash_lookup(entries_table, (const void *)&generated_entry_id) >= 0) {
		DOCA_LOG_ERR("Could not add new entry with id=%" PRIu64 ", id already exists", *generated_entry_id);
		return DOCA_ERROR_INVALID_VALUE;
	}

	result = rte_hash_add_key_data(entries_table, (const void *)generated_entry_id, (void *)entry);
	if (result != 0) {
		DOCA_LOG_ERR("Could not add new entry with id=%" PRIu64 ", to relevant entries table", *generated_entry_id);
		rte_free(generated_entry_id);
		return DOCA_ERROR_INVALID_VALUE;
	}

	result = rte_hash_add_key_data(manager->entry_id_to_pipe_id_table, (const void *)generated_entry_id, (void *)pipe_id_ptr);
	if (result != 0) {
		DOCA_LOG_ERR("Could not add new entry with id=%" PRIu64 ", to entry-to-pipe table", *generated_entry_id);
		rte_hash_del_key(entries_table, (const void *)generated_entry_id);
		return DOCA_ERROR_INVALID_VALUE;
	}

	*entry_id = *generated_entry_id;

	return DOCA_SUCCESS;
}

doca_error_t
pipes_manager_get_pipe(struct flow_pipes_manager *manager, uint64_t pipe_id, struct doca_flow_pipe **pipe)
{
	struct pipe_info *pipe_info;

	if (rte_hash_lookup_data(manager->pipe_id_to_pipe_info_table, (const void *)&pipe_id, (void **)&pipe_info) < 0)
		return DOCA_ERROR_NOT_FOUND;

	*pipe = pipe_info->pipe;

	return DOCA_SUCCESS;
}

doca_error_t
pipes_manager_get_entry(struct flow_pipes_manager *manager, uint64_t entry_id, struct doca_flow_pipe_entry **entry)
{
	struct pipe_info *pipe_info;
	struct doca_flow_pipe_entry *pipe_entry;
	uint64_t *pipe_id;

	if (rte_hash_lookup_data(manager->entry_id_to_pipe_id_table, (const void *)&entry_id, (void **)&pipe_id) < 0)
		return DOCA_ERROR_NOT_FOUND;

	if (rte_hash_lookup_data(manager->pipe_id_to_pipe_info_table, (const void *)pipe_id, (void **)&pipe_info) < 0)
		return DOCA_ERROR_NOT_FOUND;

	if (rte_hash_lookup_data(pipe_info->entries_table, (const void *)&entry_id, (void **)&pipe_entry) < 0)
		return DOCA_ERROR_NOT_FOUND;

	*entry = pipe_entry;

	return DOCA_SUCCESS;
}

doca_error_t
pipes_manager_pipe_destroy(struct flow_pipes_manager *manager, uint64_t pipe_id)
{
	struct pipe_info *pipe_info;
	struct doca_flow_pipe_entry *entry, *pipe_id_key;
	uint64_t *generated_entry_id;
	uint32_t itr = 0;
	int key_offset;

	key_offset = rte_hash_lookup_data(manager->pipe_id_to_pipe_info_table, (const void *)&pipe_id, (void *)&pipe_info);
	if (key_offset < 0) {
		DOCA_LOG_ERR("Could not remove pipe with id=%" PRIu64 ", id was not found", pipe_id);
		return DOCA_ERROR_INVALID_VALUE;
	}

	if (rte_hash_get_key_with_position(manager->pipe_id_to_pipe_info_table, key_offset, (void **)&pipe_id_key) != 0) {
		DOCA_LOG_ERR("Could not remove pipe with id=%" PRIu64 ", id was not found", pipe_id);
		return DOCA_ERROR_NOT_FOUND;
	}

	/* remove it directly from relevant port id to pipes table */
	if (rte_hash_del_key(pipe_info->port_id_to_pipes_table, (const void *)&pipe_id) < 0) {
		DOCA_LOG_ERR("Could not remove pipe with id=%" PRIu64"", pipe_id);
		return DOCA_ERROR_NOT_FOUND;
	}

	while (rte_hash_iterate(pipe_info->entries_table, (const void **)&generated_entry_id, (void **)&entry, &itr) >= 0)
		rte_hash_del_key(manager->entry_id_to_pipe_id_table, (const void *)generated_entry_id);


	rte_hash_del_key(manager->pipe_id_to_pipe_info_table, (const void *)&pipe_id);
	destroy_pipe_info(pipe_info);

	DOCA_LOG_INFO("Pipe with id %" PRIu64 " removed successfully", pipe_id);

	return DOCA_SUCCESS;
}

doca_error_t
pipes_manager_pipe_rm_entry(struct flow_pipes_manager *manager, uint64_t entry_id)
{
	struct pipe_info *pipe_info;
	uint64_t *pipe_id, *entry_id_key;
	int key_offset;

	key_offset = rte_hash_lookup_data(manager->entry_id_to_pipe_id_table, (const void *)&entry_id, (void **)&pipe_id);
	if (key_offset < 0) {
		DOCA_LOG_ERR("Could not remove entry with id=%" PRIu64 ", id was not found", entry_id);
		return DOCA_ERROR_INVALID_VALUE;
	}

	if (rte_hash_get_key_with_position(manager->entry_id_to_pipe_id_table, key_offset, (void **)&entry_id_key) != 0) {
		DOCA_LOG_ERR("Could not remove entry with id=%" PRIu64 ", id was not found", entry_id);
		return DOCA_ERROR_NOT_FOUND;
	}

	if (rte_hash_lookup_data(manager->pipe_id_to_pipe_info_table, (const void *)pipe_id, (void **)&pipe_info) < 0) {
		DOCA_LOG_ERR("Could not remove entry with id=%" PRIu64 ", relevant pipe id was not found", entry_id);
		return DOCA_ERROR_INVALID_VALUE;
	}

	if (rte_hash_del_key(manager->entry_id_to_pipe_id_table, (const void *)&entry_id) < 0) {
		DOCA_LOG_ERR("Could not remove entry with id=%" PRIu64 ", id was not found", entry_id);
		return DOCA_ERROR_NOT_FOUND;
	}

	if (rte_hash_del_key(pipe_info->entries_table, (const void *)&entry_id) < 0) {
		DOCA_LOG_ERR("Could not remove entry with id=%" PRIu64 ", id was not found", entry_id);
		return DOCA_ERROR_NOT_FOUND;
	}

	DOCA_LOG_INFO("Entry with id=%" PRIu64 " removed successfully", entry_id);

	return DOCA_SUCCESS;
}

doca_error_t
pipes_manager_pipes_flush(struct flow_pipes_manager *manager, uint16_t port_id)
{
	struct rte_hash *pipes_table;
	struct pipe_info *pipe_info;
	uint64_t *pipe_id, *generated_entry_id, *data;
	uint32_t pipe_itr = 0;
	uint32_t entry_itr;
	uint16_t *port_id_key;
	int key_offset;
	int result;

	key_offset = rte_hash_lookup_data(manager->port_id_to_pipes_id_table, (const void *)&port_id, (void **)&pipes_table);

	if (key_offset < 0) {
		DOCA_LOG_ERR("Could not find port with id=%" PRIu16 ", aborting flush", port_id);
		return DOCA_ERROR_INVALID_VALUE;
	}

	if (rte_hash_get_key_with_position(manager->port_id_to_pipes_id_table, key_offset, (void **)&port_id_key) != 0) {
		DOCA_LOG_ERR("Could not find port with id=%" PRIu16 ", aborting flush", port_id);
		return DOCA_ERROR_NOT_FOUND;
	}

	while (rte_hash_iterate(pipes_table, (const void **)&pipe_id, (void **)&data, &pipe_itr) >= 0) {

		result = rte_hash_lookup_data(manager->pipe_id_to_pipe_info_table, (const void *)pipe_id, (void **)&pipe_info);
		if (result < 0) {
			DOCA_LOG_ERR("Could not find pipe id pipe_info with id=%" PRIu64 ", aborting flush", *pipe_id);
			continue;
		}

		entry_itr = 0;
		while (rte_hash_iterate(pipe_info->entries_table, (const void **)&generated_entry_id, (void **)&data, &entry_itr) >= 0)
			rte_hash_del_key(manager->entry_id_to_pipe_id_table, (const void *)generated_entry_id);

		destroy_pipe_info(pipe_info);
		rte_hash_del_key(manager->pipe_id_to_pipe_info_table, (const void *)pipe_id);
	}

	rte_hash_free(pipes_table);
	rte_hash_del_key(manager->port_id_to_pipes_id_table, (const void **)&port_id);

	return DOCA_SUCCESS;
}
