#include <iostream>
#include <functional>
#include <uv.h>

struct MyCounter {
    int64_t counter = 0;
};

template <typename TData>
class UVManager {

private:
    uv_loop_t loop_;
    uv_idle_t idler_;

    UVManager(UVManager const & );
    UVManager &operator=(UVManager const &);

public:
    UVManager(bool bAutoRun = false) {
        uv_loop_init(&loop_);
        if(bAutoRun){
            run();
        }
    }

    UVManager(uv_idle_cb icb, const TData*data = nullptr, bool bAutoRun=true) {
        idler_.data = (void*)data;
        uv_loop_init(&loop_);
        uv_idle_init(&loop_, (uv_idle_t*)&idler_);
        uv_idle_start((uv_idle_t*)&idler_, icb);
        if(bAutoRun){
            run();
        }
    }

    virtual ~UVManager(){
        uv_loop_close(&loop_);
    }

    void run(){
        uv_run(&loop_, UV_RUN_DEFAULT);
    }
};

int main() {
    std::cout << "running the manager.." << std::endl;
    MyCounter c;
    UVManager<MyCounter> uvm(
        [](uv_idle_t* handle) {
            MyCounter* c = (MyCounter*)(handle->data);
            c->counter++;
            if (c->counter >= 10e4)
                uv_idle_stop(handle);
        }, &c);

    uvm.run();
    std::cout << c.counter << std::endl;
    return 0;
}
