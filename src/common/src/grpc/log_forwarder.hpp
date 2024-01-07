/*
 * Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
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

#ifndef LOG_FORWARDER_H_
#define LOG_FORWARDER_H_

#include <condition_variable>
#include <grpcpp/grpcpp.h>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "common.grpc.pb.h"

struct synchronized_queue {
	std::mutex queue_lock;				/* Lock for thread safe access/modification to the queue */
	std::condition_variable cond_has_records;	/* Conditional lock that will be notified every time a new log
							 * was added to the queue
							 */
	std::queue< std::string > pending_records;	/* The log messages of the queue */
};

struct clients_pool {
	std::mutex lock;										/* Pool lock - used for synchronizing access to the pool member */
	std::vector< std::pair<grpc::ServerWriter< LogRecord > *, std::condition_variable * > > pool;	/* Holds pairs of client stream and their matching conditional lock.
													 * The stream is used for stream log records to a subscribed client.
													 * The conditional lock is used to notify the stream owner when the
													 * stream can be released
													 */
};

/*
 * Blocks caller until "queue" is not empty
 *
 * @queue [in]: Sync queue
 */
void synchronized_queue_block_until_has_logs(struct synchronized_queue *queue);

/*
 * Removes a record from the queue and returns it
 *
 * @queue [in]: Sync queue
 * @return: The removed message
 *
 * @NOTE: If "teardown_server_sessions" was called or if queue is empty, the empty string "" will be returned
 */
std::string synchronized_queue_dequeue(struct synchronized_queue *queue);

/*
 * Insert a new log record into "queue"
 *
 * @queue [in]: Sync queue
 * @msg [in]: The message to insert into "queue"
 */
void synchronized_queue_enqueue(struct synchronized_queue *queue, std::string msg);

/*
 * Consumes log records from "queue" and sends them to all clients in "clients" in an loop until
 * "teardown_server_sessions" is called.
 *
 * @queue [in]: Sync queue of log records
 * @clients [in]: Pool of clients for forwarding the log to. Clients can be added and removed while this method executes
 */
void forward_log_records(struct synchronized_queue *queue, struct clients_pool *clients);

/*
 * Adds a stream to the pool and waits on a global conditional lock until the stream is removed from the pool.
 *
 * @clients [in]: Clients pool
 * @writer [in]: New client stream
 * @return: true on success and false otherwise
 *
 * @NOTE: This function will not return until "teardown_server_sessions" is called
 */
bool subscribe_client(struct clients_pool *clients, grpc::ServerWriter< LogRecord > *writer);

/*
 * Stops all forwarding loops of and releases them given arguments resources
 *
 * @queue [in]: Sync queue of log records to be released
 * @clients [in]: Pool of clients to be released
 */
void teardown_server_sessions(struct synchronized_queue *queue, struct clients_pool *clients);

#endif /* LOG_FORWARDER_H_ */
