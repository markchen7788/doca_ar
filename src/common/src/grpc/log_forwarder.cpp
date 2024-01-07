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

#include "log_forwarder.hpp"

/* Boolean for ending session with a client */
static volatile bool is_server_active = true;

void
synchronized_queue_block_until_has_logs(struct synchronized_queue *queue)
{
	std::mutex is_empty_mutex;
	std::unique_lock<std::mutex> lock(is_empty_mutex);
	queue->cond_has_records.wait(lock, [&queue] {
		return !queue->pending_records.empty() || !is_server_active; });
}

std::string
synchronized_queue_dequeue(struct synchronized_queue *queue)
{
	std::string msg;
	/* Returning "" instead of NULL because std::string ctor doesn't accept NULL */
	if (!is_server_active)
		return "";
	queue->queue_lock.lock();
	if (queue->pending_records.empty()) {
		queue->queue_lock.unlock();
		return "";
	}
	msg = queue->pending_records.front();
	queue->pending_records.pop();
	queue->queue_lock.unlock();
	return msg;
}

void
synchronized_queue_enqueue(struct synchronized_queue *queue, std::string msg)
{
	queue->queue_lock.lock();
	queue->pending_records.push(msg);
	queue->queue_lock.unlock();
	queue->cond_has_records.notify_one();
}

void
forward_log_records(struct synchronized_queue *queue, struct clients_pool *clients)
{
	/*
	 * gRPC are saying write is a blocking operation. So it is needed to be done a different
	 * thread than the "bump on the wire" thread.
	 */
	LogRecord response;
	std::string msg;
	do {
		/* Wait until a new log record has formed or the client closed the session */
		synchronized_queue_block_until_has_logs(queue);

		/* Get log message from queue */
		msg = synchronized_queue_dequeue(queue);

		/* Send message to client, if fails client most likely closed the connection */
		clients->lock.lock();
		/* Need to check if server is active after the lock because it can't change during the loop */
		if (!is_server_active) {
			clients->lock.unlock();
			break;
		}
		/* Using set_log_line inside protected segment to avoid another if statement */
		response.set_log_line(msg);
		auto it = clients->pool.begin();
		while (it != clients->pool.end()) {
			grpc::ServerWriter<LogRecord> *tmp = it->first;
			if (tmp->Write(response))
				++it;
			else { /* Connection is corrupted */
				it->second->notify_one();
				it = clients->pool.erase(it);
			}
		}
		clients->lock.unlock();
	} while (is_server_active);
}

bool
subscribe_client(struct clients_pool *clients, grpc::ServerWriter<LogRecord> *writer)
{
	/* Creating a lock for the "sleeping until connection is stale" */
	static std::mutex thread_mutex;
	std::unique_lock<std::mutex> lock(thread_mutex);
	static std::condition_variable wait_for_connection_close;

	/* Join the pool */
	clients->lock.lock();
	if (!is_server_active) {
		clients->lock.unlock();
		return false;
	}
	clients->pool.push_back(std::make_pair(writer, &wait_for_connection_close));
	clients->lock.unlock();

	/* Sleep until connection ends */
	wait_for_connection_close.wait(lock);

	return true;
}

void
teardown_server_sessions(struct synchronized_queue *queue, struct clients_pool *clients)
{
	clients->lock.lock(); /* This is a work around gRPC behavior in which the server won't terminate
				   * if there's an unfinished gRPC call from a client. Even if
				   * server->Shutdown() is called.
				   */
	is_server_active = false;
	queue->cond_has_records.notify_one();
	for (auto stream_and_lock : clients->pool)
		stream_and_lock.second->notify_one();
	clients->lock.unlock();
}
