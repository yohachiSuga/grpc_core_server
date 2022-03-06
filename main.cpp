#include "helloworld.pb.h"
#include "queue.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <grpc.h>

constexpr char helloworld_end[] = "/helloworld.Greeter/SayHello";

grpc_byte_buffer *string_to_byte_buffer(const char *string, size_t length) {
    grpc_slice slice = grpc_slice_from_copied_buffer(string, length);
    grpc_byte_buffer *buffer = grpc_raw_byte_buffer_create(&slice, 1);
    grpc_slice_unref(slice);
    return buffer;
}

void grpc_metadata_array_init(grpc_metadata_array *array) { memset(array, 0, sizeof(grpc_metadata_array)); }

typedef struct server_ctx {
    grpc_server *server;
    grpc_completion_queue *cq;
    void *head;
} server_ctx;

typedef struct server_queue_item {
    char *method;
    char *body;
    size_t body_length;
    LIST_ENTRY(server_queue_item) entries;
} server_queue_item;

void *server_thread(void *data) {
    server_ctx *ctx = (server_ctx *)data;

    gpr_timespec ts = {.tv_sec = 1, .tv_nsec = 0, .clock_type = GPR_TIMESPAN};

    // TODO: can process only one request..
    while (1) {
        grpc_call *call;
        grpc_call_details details;
        details.host = grpc_empty_slice();
        details.method = grpc_empty_slice();
        grpc_metadata_array request_metadata;
        grpc_metadata_array_init(&request_metadata);

        grpc_server_request_call(ctx->server, &call, &details, &request_metadata, ctx->cq, ctx->cq, (void *)call);

        bool is_loop = true;
        while (is_loop) {
            grpc_event e = grpc_completion_queue_pluck(ctx->cq, (void *)call, ts, nullptr);
            switch (e.type) {
            case GRPC_QUEUE_TIMEOUT:
                fprintf(stderr, "poll queue, timeout\n");
                break;

            default:
                fprintf(stderr, "received call\n");
                fprintf(stderr, "method:%s, host:%s\n", grpc_slice_to_c_string(details.method), grpc_slice_to_c_string(details.host));

                if (strcmp(helloworld_end, grpc_slice_to_c_string(details.method)) == 0) {
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
                    helloworld::HelloReply reply;
                    char res_msg[] = "response hello";
                    reply.set_message(res_msg);
                    std::string body = reply.SerializeAsString();
                    grpc_slice slice = grpc_slice_from_copied_buffer(body.c_str(), body.length());
                    grpc_byte_buffer *buf = grpc_raw_byte_buffer_create(&slice, 1);

                    op->op = GRPC_OP_SEND_MESSAGE;
                    op->data.send_message.send_message = buf;

                    // TODO: need to push queue

                    // send status
                    op++;
                    op->op = GRPC_OP_SEND_STATUS_FROM_SERVER;
                    op->data.send_status_from_server.trailing_metadata_count = 0;
                    op->data.send_status_from_server.status = GRPC_STATUS_OK;
                    grpc_slice status_details = grpc_slice_from_static_string("xyz");
                    op->data.send_status_from_server.status_details = &status_details;
                    op->flags = 0;
                    op->reserved = nullptr;
                    // end reponse close
                    op++;
                    op->op = GRPC_OP_RECV_CLOSE_ON_SERVER;
                    op->flags = 0;
                    op->reserved = nullptr;

                    grpc_call_start_batch(call, ops, static_cast<size_t>(op - ops), (void *)100, nullptr);

                    is_loop = false;
                } else {
                    fprintf(stderr, "rpc called but unknown rpc method\n");
                }
            }
        }
        fprintf(stderr, "finish handle rpc\n");
    }

    return nullptr;
}

int main(int argc, char const *argv[]) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    /* code */
    grpc_init();
    const char *version = grpc_version_string();
    if (version == nullptr) {
        return -1;
    } else {
        fprintf(stderr, "version:%s\n", version);
    }

    // server builder
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

    // server start
    grpc_server_start(server);

    SLIST_HEAD(server_queue_head, server_queue_item) head;
    SLIST_INIT(&head);

    // currently only one thread and proc unary call
    pthread_t work_th;
    server_ctx ctx;
    ctx.server = server;
    ctx.cq = cq;
    ctx.head = (void *)&head;
    ret = pthread_create(&work_th, nullptr, server_thread, (void *)&ctx);
    if (ret != 0) {
        fprintf(stderr, "thread_creation error\n");
    }

    pthread_join(work_th, nullptr);

    grpc_server_shutdown_and_notify(server, cq, nullptr);

    grpc_server_destroy(server);
    grpc_completion_queue_shutdown(cq);
    grpc_completion_queue_destroy(cq);
    grpc_shutdown();

    google::protobuf::ShutdownProtobufLibrary();

    return 0;
}
