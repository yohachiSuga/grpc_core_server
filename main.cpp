#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <grpc.h>

grpc_byte_buffer *string_to_byte_buffer(char *string, size_t length) {
    grpc_slice slice = grpc_slice_from_copied_buffer(string, length);
    grpc_byte_buffer *buffer = grpc_raw_byte_buffer_create(&slice, 1);
    grpc_slice_unref(slice);
    return buffer;
}

int main(int argc, char const *argv[]) {
    /* code */
    grpc_init();
    const char *version = grpc_version_string();
    if (version == nullptr) {
        return -1;
    } else {
        fprintf(stderr, "version:%s\n", version);
    }

    grpc_server *server = grpc_server_create(nullptr, nullptr);
    if (server == nullptr) {
        fprintf(stderr, "grpc_server_create failed %d\n", __LINE__);
        return -1;
    }

    grpc_completion_queue *cq = grpc_completion_queue_create_for_pluck(nullptr);
    grpc_server_register_completion_queue(server, cq, nullptr);

    const char addr[] = "[::]:50051";
    int ret = grpc_server_add_insecure_http2_port(server, addr);
    if (ret == 0) {
        fprintf(stderr, "cannot bind addr %s\n", addr);
        return -1;
    }

    // const char *method = "";
    // const char *host = "";
    // void *ret_pointer = grpc_server_register_method(server, method, host, GRPC_SRM_PAYLOAD_READ_INITIAL_BYTE_BUFFER, 0);
    // if (ret_pointer == nullptr) {
    //     fprintf(stderr, "regsitration error\n");
    //     return -1;
    // }
    grpc_server_start(server);

    grpc_call *call;
    grpc_call_details *details;
    grpc_metadata_array *request_metadata;
    grpc_server_request_call(server, &call, details, request_metadata, cq, cq, (void *)1);
    gpr_timespec ts = {.tv_sec = 1, .tv_nsec = 0, .clock_type = GPR_TIMESPAN};
    bool is_loop = true;
    while (is_loop) {
        grpc_event e = grpc_completion_queue_pluck(cq, (void *)1, ts, nullptr);
        switch (e.type) {
        case GRPC_QUEUE_TIMEOUT:
            fprintf(stderr, "poll queue, timeout\n");
            break;

        default:
            fprintf(stderr, "received call\n");
            fprintf(stderr, "method:%s, host:%s\n", grpc_slice_to_c_string(details->method), grpc_slice_to_c_string(details->host));

            // send response
            grpc_op ops[6];
            memset(ops, 0, sizeof(ops));
            grpc_op *op = ops;
            op->op = GRPC_OP_SEND_INITIAL_METADATA;
            op->data.send_initial_metadata.count = 0;
            op->flags = 0;
            op->reserved = nullptr;

            // send message
            op++;
            char res[] = "hello";
            grpc_byte_buffer *buf = string_to_byte_buffer(res, strlen(res));

            op->op = GRPC_OP_SEND_MESSAGE;
            op->data.send_message.send_message = buf;

            // send status
            op++;
            op->op = GRPC_OP_SEND_STATUS_FROM_SERVER;
            op->data.send_status_from_server.trailing_metadata_count = 0;
            op->data.send_status_from_server.status = GRPC_STATUS_UNIMPLEMENTED;
            grpc_slice status_details = grpc_slice_from_static_string("xyz");
            op->data.send_status_from_server.status_details = &status_details;
            op->flags = 0;
            op->reserved = nullptr;
            op++;
            op->op = GRPC_OP_RECV_CLOSE_ON_SERVER;
            op->flags = 0;
            op->reserved = nullptr;

            grpc_call_start_batch(call, ops, static_cast<size_t>(op - ops), (void *)100, nullptr);

            is_loop = false;
            break;
        }
    }

    grpc_server_shutdown_and_notify(server, cq, nullptr);

    grpc_server_destroy(server);
    grpc_completion_queue_shutdown(cq);
    grpc_completion_queue_destroy(cq);
    grpc_shutdown();

    return 0;
}
