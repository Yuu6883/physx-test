#pragma once

#include <stdio.h>
#include <csignal>
#include <string_view>
#include <uv.h>
#include <memory>

using std::string_view;

static inline std::unique_ptr<char[]> pool(new char[65536]);

class repl {

    static inline bool closing;
    static inline uv_tty_t tty;
    static inline uv_pipe_t stdin_pipe;
    static inline uv_signal_t signal_handle;
    static inline bool isTTY;

    static void on_sigint(uv_signal_t* handle, int signal) {
        printf("SIGINT\n");
        auto loop = handle->loop;
        uv_walk(loop, [](uv_handle_t* handle, void* arg) {
            uv_close(handle, nullptr);
        }, nullptr);
        uv_loop_close(loop);
    }

    static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
        *buf = uv_buf_init(pool.get(), suggested_size);
    }

    static void read_stdin(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
        if (nread < 0){
            if (nread == UV_EOF){
                // end of file
                std::raise(SIGINT);
            }
        } else if (nread > 0) {
            string_view view = string_view(buf->base, nread).substr(0, nread - 1);
            
            if (view == "exit") {
                std::raise(SIGINT);
            } else {
                // process command
            }
        }
    }
public:
    static void run() {
        closing = false;
        
        uv_loop_t* loop = uv_default_loop();

        uv_signal_init(loop, &signal_handle);
        uv_signal_start(&signal_handle, on_sigint, SIGINT);

        isTTY = uv_guess_handle(0) == UV_TTY;

        if (isTTY) {
            uv_tty_init(loop, &tty, 0, 1);
            uv_tty_set_mode(&tty, UV_TTY_MODE_NORMAL);
            uv_read_start((uv_stream_t*) &tty, alloc_buffer, read_stdin);
        } else {
            uv_pipe_init(loop, &stdin_pipe, 0);
            uv_pipe_open(&stdin_pipe, 0);
            uv_read_start((uv_stream_t*) &stdin_pipe, alloc_buffer, read_stdin);
        }
    }
};