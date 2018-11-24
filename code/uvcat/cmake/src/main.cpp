#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <uv.h>
#include <memory>
#include <iostream>

void on_read(uv_fs_t *req);

template <typename TData>
class UVManager :public TData {

public:
    uv_loop_t loop_;
    uv_loop_t* ext_loop;
    uv_idle_t idler_;

private:
    UVManager(UVManager const & );
    UVManager &operator=(UVManager const &);

public:
    UVManager(uv_loop_t* pLoop = NULL, bool bAutoRun = false) : ext_loop(pLoop) {
        if(!pLoop){
            uv_loop_init(&loop_);
        }

        if(bAutoRun){
            run();
        }
    }

    UVManager(uv_idle_cb icb, bool bAutoRun=true) {
        idler_.data = (void*)this;
        uv_loop_init(&loop_);
        uv_idle_init(&loop_, (uv_idle_t*)&idler_);
        uv_idle_start((uv_idle_t*)&idler_, icb);
        if(bAutoRun){
            run();
        }
    }

    virtual ~UVManager(){
        uv_loop_close((ext_loop ? ext_loop : &loop_));
    }

    void run(){
        uv_run((ext_loop ? ext_loop : &loop_), UV_RUN_DEFAULT);
    }

    uv_loop_t* loop() {
        return (ext_loop ? ext_loop : &loop_);
    }
};

template <typename TData>
struct RequestPack {

    uv_fs_t open_req;
    uv_fs_t read_req;
    uv_fs_t write_req;
    TData* data_;

public:
    RequestPack(TData* pdata=nullptr) : data_(pdata) {
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
        RequestPack<char>* rpp = static_cast<RequestPack<char>*>(req->data);
        uv_fs_read(uv_default_loop(), &(rpp->read_req), rpp->open_req.result, &iov, 1, -1, on_read);
        std::cout << "wrote.." << std::endl;
    }
}

void on_read(uv_fs_t *req) {
    RequestPack<char>* rpp = static_cast<RequestPack<char>*>(req->data);

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
    std::cout << "on open" << std::endl;
    RequestPack<char>* rpp = static_cast<RequestPack<char>*>(req->data);

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
    UVManager<RequestPack<char>> uvm(uv_default_loop(), false);
    uvm.data_ = buffer;
    std::cout <<  "even getting here?" << std::endl;
    uv_fs_open(uvm.loop(), &uvm.open_req, argv[1], O_RDONLY, 0, on_open);
    std::cout <<  "how about here?" << std::endl;
    uvm.run();

    return 0;
}
