#pragma once

#include <unistd.h>

namespace TUI::Common::RAII
{
    class Fd
    {
    public:
        explicit Fd(int fd) : _fd(fd) {}
        ~Fd() { if (_fd != -1) close(_fd); }

        Fd(const Fd&) = delete;
        Fd& operator=(const Fd&) = delete;

        Fd(Fd&& other) noexcept : _fd(other._fd) { other._fd = -1; }
        Fd& operator=(Fd&& other) noexcept
        {
            if (this != &other)
            {
                if (_fd != -1) close(_fd);
                _fd = other._fd;
                other._fd = -1;
            }
            return *this;
        }

        int Get() const { return _fd; }
        void Close()
        {
            if (_fd != -1)
            {
                close(_fd);
                _fd = -1;
            }
        }
    private:
        int _fd{-1};
    };
}
