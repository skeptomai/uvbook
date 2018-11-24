#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <uv.h>
#include <memory>
#include <iostream>

void on_open(uv_fs_t *req);
void on_read(uv_fs_t *req);
void on_write(uv_fs_t *req);


template<typename TBufType>
class MinBuf {

public:
    MinBuf(int btsize ) : data_(std::shared_ptr<TBufType>(new TBufType[btsize], std::default_delete<TBufType[]>())) {
        iov_ = uv_buf_init(data_.get(), btsize);
    }

    std::shared_ptr<TBufType> data_;
    uv_buf_t iov_;
};

template <typename TData>
struct RequestPack {

    uv_fs_t open_req;
    uv_fs_t read_req;
    uv_fs_t write_req;
    MinBuf<char> mb_;

public:
    RequestPack() : mb_(1024) {
        open_req.data = static_cast<TData*>(this);
        read_req.data = static_cast<TData*>(this);
        write_req.data = static_cast<TData*>(this);
    }

    ~RequestPack() {
        uv_fs_req_cleanup(&open_req);
        uv_fs_req_cleanup(&read_req);
        uv_fs_req_cleanup(&write_req);
    }

    uv_buf_t* iov() {return &mb_.iov_;}
};

class UVManager : public RequestPack<UVManager> {

public:
    uv_loop_t loop_;
    uv_loop_t* ext_loop;

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
        uv_loop_init(&loop_);
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

void on_write(uv_fs_t *req) {
    std::cout << "in on_write"  << std::endl;
    auto uvmp = static_cast<UVManager*>(req->data);

    if (req->result < 0) {
        fprintf(stderr, "Write error: %s\n", uv_strerror((int)req->result));
    }
    else {
        std::cout << "writing" << std::endl;
        std::cout << "cast.." << std::endl;
        uv_fs_read(uvmp->loop(), &(uvmp->read_req), uvmp->open_req.result, uvmp->iov(), 1, -1, on_read);
        std::cout << "wrote.." << std::endl;
    }
}

void on_read(uv_fs_t *req) {
    std::cout << "on_read called" << std::endl;
    auto uvmp = static_cast<UVManager*>(req->data);

    if (req->result < 0) {
        fprintf(stderr, "Read error: %s\n", uv_strerror(req->result));
    }
    else if (req->result == 0) {
        std::cout << "in on_read closing" << std::endl;
        // synchronous
        uv_fs_t close_req;
        uv_fs_close(uvmp->loop(), &close_req, uvmp->open_req.result, NULL);
        std::cout << "in on_read closed" << std::endl;
    }
    else if (req->result > 0) {
        std::cout << "in on_read, calling write" << std::endl;
        uvmp->iov()->len = req->result;
        uv_fs_write(uvmp->loop(), &(uvmp->write_req), 1, uvmp->iov(), 1, -1, on_write);
        std::cout << "in on_read, wrote" << std::endl;
    }
}

void on_open(uv_fs_t *req) {
    std::cout << "on open" << std::endl;
    auto uvmp = static_cast<UVManager*>(req->data);

    if (req->result >= 0) {

        uv_fs_read(uvmp->loop(), &(uvmp->read_req), req->result,
                   uvmp->iov(), 1, -1, on_read);
    }
    else {
        fprintf(stderr, "error opening file: %s\n", uv_strerror((int)req->result));
    }
}

int main(int argc, char **argv) {
    UVManager uvm(uv_default_loop(), false);
    std::cout <<  "even getting here?" << std::endl;
    uv_fs_open(uvm.loop(), &uvm.open_req, argv[1], O_RDONLY, 0, on_open);
    std::cout <<  "how about here?" << std::endl;
    uvm.run();

    return 0;
}
