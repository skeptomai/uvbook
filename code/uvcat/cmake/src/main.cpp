#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <uv.h>
#include <iostream>

void on_read(uv_fs_t *req);


struct RequestPack {

    uv_fs_t open_req;
    uv_fs_t read_req;
    uv_fs_t write_req;


public:
    RequestPack() {
        open_req.data = this;
        read_req.data = this;
        write_req.data = this;
    }

    ~RequestPack() {
        uv_fs_req_cleanup(&open_req);
        uv_fs_req_cleanup(&read_req);
        uv_fs_req_cleanup(&write_req);
    }
};


static char buffer[1024];

static uv_buf_t iov;

template<typename TBufType>
class MinBuf {

public:
    MinBuf(TBufType* bt, double btsize ){
        iov_ = uv_buf_init(bt, btsize);
    }

    uv_buf_t iov_;
};

void on_write(uv_fs_t *req) {
    std::cout << "in on_write"  << std::endl;
    if (req->result < 0) {
        fprintf(stderr, "Write error: %s\n", uv_strerror((int)req->result));
    }
    else {
        std::cout << "writing" << std::endl;
        RequestPack* rpp = static_cast<RequestPack*>(req->data);
        uv_fs_read(uv_default_loop(), &(rpp->read_req), rpp->open_req.result, &iov, 1, -1, on_read);
        std::cout << "wrote.." << std::endl;
    }
}

void on_read(uv_fs_t *req) {
    RequestPack* rpp = static_cast<RequestPack*>(req->data);

    std::cout << "on_read called" << std::endl;

    if (req->result < 0) {
        fprintf(stderr, "Read error: %s\n", uv_strerror(req->result));
    }
    else if (req->result == 0) {
        std::cout << "in on_read closing" << std::endl;
        // synchronous
        uv_fs_t close_req;
        uv_fs_close(uv_default_loop(), &close_req, rpp->open_req.result, NULL);
        std::cout << "in on_read closed" << std::endl;
    }
    else if (req->result > 0) {
        std::cout << "in on_read, calling write" << std::endl;
        iov.len = req->result;
        uv_fs_write(uv_default_loop(), &(rpp->write_req), 1, &iov, 1, -1, on_write);
        std::cout << "in on_read, wrote" << std::endl;
    }
}

void on_open(uv_fs_t *req) {
    RequestPack* rpp = static_cast<RequestPack*>(req->data);

    if (req->result >= 0) {
        MinBuf<char> mb(buffer, sizeof(buffer));
        iov = mb.iov_;
        uv_fs_read(uv_default_loop(), &(rpp->read_req), req->result,
                   &iov, 1, -1, on_read);
    }
    else {
        fprintf(stderr, "error opening file: %s\n", uv_strerror((int)req->result));
    }
}

int main(int argc, char **argv) {
    RequestPack rp;
    uv_fs_open(uv_default_loop(), &rp.open_req, argv[1], O_RDONLY, 0, on_open);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    return 0;
}
