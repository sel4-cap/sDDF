/*
 * Copyright 2024, UNSW
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sddf/util/fence.h>

/* Maximum number of slots in the request queue. Can be configured. */
#define BLK_REQ_QUEUE_SIZE 1024
/* Maximum number of slots in the response queue. Can be configured. */
#define BLK_RESP_QUEUE_SIZE 1024
/* Device serial number max string length */
#define BLK_MAX_SERIAL_NUMBER 63

typedef struct blk_storage_info {
    char serial_number[BLK_MAX_SERIAL_NUMBER + 1]; 
    bool read_only;
    bool ready;
    uint16_t blocksize;
    uint16_t queue_depth;
    uint16_t cylinders, heads, blocks; /* geometry to guide FS layout */
    uint64_t size; /* number of blocksize units */
} blk_storage_info_t;

/* Request code for block */
typedef enum blk_request_code {
    READ_BLOCKS,
    WRITE_BLOCKS,
    FLUSH,
    BARRIER,
} blk_request_code_t;

/* Response status for block */
typedef enum blk_response_status {
    SUCCESS,
    SEEK_ERROR
} blk_response_status_t;

/* Request struct contained in request queue */
typedef struct blk_request {
    blk_request_code_t code; /* request code */
    uintptr_t addr; /* encoded dma address of data */
    uint32_t block_number; /* block number to read/write to */
    uint16_t count; /* number of blocks to read/write */
    uint32_t id; /* stores request ID */
} blk_request_t;

/* Response struct contained in response queue */
typedef struct blk_response {
    blk_response_status_t status; /* response status */
    uintptr_t addr; /* encoded dma address of data */
    uint16_t count; /* number of blocks allocated for corresponding request */
    uint16_t success_count; /* number of blocks successfully read/written */
    uint32_t id; /* stores corresponding request ID */
} blk_response_t;

/* Circular buffer containing requests */
typedef struct blk_req_queue {
    uint32_t write_idx;
    uint32_t read_idx;
    uint32_t size; /* number of slots in request queue */
    bool plugged; /* prevent requests from being dequeued when plugged */
    blk_request_t buffers[BLK_REQ_QUEUE_SIZE];
    bool in_progress; /* is xfer in progress? (temp) */
} blk_req_queue_t;

/* Circular buffer containing responses */
typedef struct blk_resp_queue {
    uint32_t write_idx;
    uint32_t read_idx;
    uint32_t size; /* number of slots in response queue */
    blk_response_t buffers[BLK_RESP_QUEUE_SIZE];
} blk_resp_queue_t;

/* A queue handle for queueing/dequeueing request and responses */
typedef struct blk_queue_handle {
    blk_req_queue_t *req_queue;
    blk_resp_queue_t *resp_queue;
} blk_queue_handle_t;

/**
 * Initialise the shared queues.
 *
 * @param h queue handle to use.
 * @param request pointer to request queue in shared memory.
 * @param response pointer to response queue in shared memory.
 * @param buffer_init true indicates the read and write indices in shared memory need to be initialised.
 *                    false indicates they do not. Only one side of the shared memory regions needs to do this.
 * @param request_size number of slots in request queue.
 * @param response_size number of slots in response queue.
 */
void blk_queue_init(blk_queue_handle_t *h,
                        blk_req_queue_t *request,
                        blk_resp_queue_t *response,
                        bool buffer_init,
                        uint32_t request_size,
                        uint32_t response_size);

/**
 * Check if the request queue is empty.
 *
 * @param h queue handle containing request queue.
 *
 * @return true indicates the request queue is empty, false otherwise.
 */
static inline bool blk_req_queue_empty(blk_queue_handle_t *h)
{
    return !((h->req_queue->write_idx - h->req_queue->read_idx) % h->req_queue->size);
}

/**
 * Check if the response queue is empty.
 *
 * @param h queue handle containing response queue.
 *
 * @return true indicates the response queue is empty, false otherwise.
 */
static inline bool blk_resp_queue_empty(blk_queue_handle_t *h)
{
    return !((h->resp_queue->write_idx - h->resp_queue->read_idx) % h->resp_queue->size);
}

/**
 * Check if the request queue is full.
 *
 * @param h queue handle containing request queue.
 *
 * @return true indicates the request queue is full, false otherwise.
 */
static inline bool blk_req_queue_full(blk_queue_handle_t *h)
{
    return !((h->req_queue->write_idx - h->req_queue->read_idx + 1) % h->req_queue->size);
}

/**
 * Check if the response queue is full.
 *
 * @param h queue handle containing response queue.
 *
 * @return true indicates the response queue is full, false otherwise.
 */
static inline bool blk_resp_queue_full(blk_queue_handle_t *h)
{
    return !((h->resp_queue->write_idx - h->resp_queue->read_idx + 1) % h->resp_queue->size);
}

/**
 * Get the number of elements in a request queue.
 *
 * @param h queue handle containing request and response queues.
 *
 * @return number of elements in the queue.
 */
static inline int blk_req_queue_size(blk_queue_handle_t *h)
{
    return (h->req_queue->write_idx - h->req_queue->read_idx);
}

/**
 * Get the number of elements in a response queue.
 *
 * @param h queue handle containing request and response queues.
 *
 * @return number of elements in the queue.
 */
static inline int blk_resp_queue_size(blk_queue_handle_t *h)
{
    return (h->resp_queue->write_idx - h->resp_queue->read_idx);
}

/**
 * Enqueue an element into the request queue.
 *
 * @param h queue handle containing request queue to enqueue to.
 * @param code request code.
 * @param addr encoded dma address of data to read/write.
 * @param block_number block number to read/write to.
 * @param count the number of blocks to read/write
 * @param id request ID to identify this request.
 *
 * @return -1 when request queue is full, 0 on success.
 */
static inline int blk_enqueue_req(blk_queue_handle_t *h,
                                        blk_request_code_t code,
                                        uintptr_t addr,
                                        uint32_t block_number,
                                        uint16_t count,
                                        uint32_t id)
{
    if (blk_req_queue_full(h)) {
        return -1;
    }

    h->req_queue->buffers[h->req_queue->write_idx % h->req_queue->size].code = code;
    h->req_queue->buffers[h->req_queue->write_idx % h->req_queue->size].addr = addr;
    h->req_queue->buffers[h->req_queue->write_idx % h->req_queue->size].block_number = block_number;
    h->req_queue->buffers[h->req_queue->write_idx % h->req_queue->size].count = count;
    h->req_queue->buffers[h->req_queue->write_idx % h->req_queue->size].id = id;

    THREAD_MEMORY_RELEASE();
    h->req_queue->write_idx++;

    return 0;
}

/**
 * Enqueue an element into the response queue.
 * This indicates the request has been processed and a response is ready.
 *
 * @param h queue handle containing response queue to enqueue to.
 * @param status response status.
 * @param addr pointer to encoded dma address of data.
 * @param count number of blocks allocated for corresponding request
 * @param success_count number of blocks successfully read/written
 * @param id request ID to identify which request the response is for.
 *
 * @return -1 when response queue is full, 0 on success.
 */
static inline int blk_enqueue_resp(blk_queue_handle_t *h,
                                        blk_response_status_t status,
                                        uintptr_t addr,
                                        uint16_t count,
                                        uint16_t success_count,
                                        uint32_t id)
{
    if (blk_resp_queue_full(h)) {
        return -1;
    }

    h->resp_queue->buffers[h->resp_queue->write_idx % h->resp_queue->size].status = status;
    h->resp_queue->buffers[h->resp_queue->write_idx % h->resp_queue->size].addr = addr;
    h->resp_queue->buffers[h->resp_queue->write_idx % h->resp_queue->size].count = count;
    h->resp_queue->buffers[h->resp_queue->write_idx % h->resp_queue->size].success_count = success_count;
    h->resp_queue->buffers[h->resp_queue->write_idx % h->resp_queue->size].id = id;

    THREAD_MEMORY_RELEASE();
    h->resp_queue->write_idx++;

    return 0;
}

/**
 * Dequeue an element from the request queue.
 *
 * @param h queue handle containing request queue to dequeue from.
 * @param code pointer to request code.
 * @param addr pointer to encoded dma address of data.
 * @param block_number pointer to  block number to read/write to.
 * @param count pointer to number of blocks to read/write.
 * @param id pointer to store request ID.
 *
 * @return -1 when request queue is empty, 0 on success.
 */
static inline int blk_dequeue_req(blk_queue_handle_t *h,
                                        blk_request_code_t *code,
                                        uintptr_t *addr,
                                        uint32_t *block_number,
                                        uint16_t *count,
                                        uint32_t *id)
{
    if (blk_req_queue_empty(h)) {
        return -1;
    }

    *code = h->req_queue->buffers[h->req_queue->read_idx % h->req_queue->size].code;
    *addr = h->req_queue->buffers[h->req_queue->read_idx % h->req_queue->size].addr;
    *block_number = h->req_queue->buffers[h->req_queue->read_idx % h->req_queue->size].block_number;
    *count = h->req_queue->buffers[h->req_queue->read_idx % h->req_queue->size].count;
    *id = h->req_queue->buffers[h->req_queue->read_idx % h->req_queue->size].id;

    THREAD_MEMORY_RELEASE();
    h->req_queue->read_idx++;

    return 0;
}

/**
 * Dequeue an element from a response queue.
 *
 * @param h queue handle containing response queue to dequeue from.
 * @param status pointer to response status.
 * @param addr pointer to encoded dma address of data.
 * @param count pointer to number of blocks allocated for corresponding request
 * @param success_count pointer to number of blocks successfully read/written
 * @param id pointer to stoqueue request ID to idenfity which request this response is for.
 * @return -1 when response queue is empty, 0 on success.
 */
static inline int blk_dequeue_resp(blk_queue_handle_t *h,
                                        blk_response_status_t *status,
                                        uintptr_t *addr,
                                        uint16_t *count,
                                        uint16_t *success_count,
                                        uint32_t *id)
{
    if (blk_resp_queue_empty(h)) {
        return -1;
    }

    *status = h->resp_queue->buffers[h->resp_queue->read_idx % h->resp_queue->size].status;
    *addr = h->resp_queue->buffers[h->resp_queue->read_idx % h->resp_queue->size].addr;
    *count = h->resp_queue->buffers[h->resp_queue->read_idx % h->resp_queue->size].count;
    *success_count = h->resp_queue->buffers[h->resp_queue->read_idx % h->resp_queue->size].success_count;
    *id = h->resp_queue->buffers[h->resp_queue->read_idx % h->resp_queue->size].id;

    THREAD_MEMORY_RELEASE();
    h->resp_queue->read_idx++;

    return 0;
}

/**
 * Set the plug of the request queue to true.
 *
 * @param h queue handle containing request queue to check for plug.
*/
static inline void blk_req_queue_plug(blk_queue_handle_t *h) {
    h->req_queue->plugged = true;
}

/**
 * Set the plug of the request queue to false.
 *
 * @param h queue handle containing request queue to check for plug.
*/
static inline void blk_req_queue_unplug(blk_queue_handle_t *h) {
    h->req_queue->plugged = false;
}

/**
 * Check the current value of the request queue plug.
 *
 * @param h queue handle containing request queue to check for plug.
 *
 * @return true when request queue is plugged, false when unplugged.
*/
static inline bool blk_req_queue_plugged(blk_queue_handle_t *h) {
    return h->req_queue->plugged;
}

