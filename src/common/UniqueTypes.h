#pragma once

#include <unistd.h>
#include <tev-cpp/Tev.h>

namespace TUI::Common::Unique
{
    class Fd
    {
    public:
        explicit Fd(int fd) : _fd(fd) {}
        ~Fd()
        {
            if (_fd != -1)
            {
                close(_fd);
            }
        }

        Fd(const Fd&) = delete;
        Fd& operator=(const Fd&) = delete;

        Fd(Fd&& other) noexcept
            : _fd(other._fd)
        { 
            other._fd = -1;
        }

        Fd& operator=(Fd&& other) noexcept
        {
            if (this != &other)
            {
                if (_fd != -1)
                {
                    close(_fd);
                }
                _fd = other._fd;
                other._fd = -1;
            }
            return *this;
        }

        operator int() const
        {
            return _fd;
        }
    private:
        int _fd{-1};
    };

    class TimeoutHandle
    {
    public:
        TimeoutHandle(Tev& tev, Tev::TimeoutHandle handle)
            : _tev(tev), _handle(handle)
        {
        }
        ~TimeoutHandle()
        {
            if (_handle != 0)
            {
                _tev.ClearTimeout(_handle);
            }
        }
        TimeoutHandle(const TimeoutHandle&) = delete;
        TimeoutHandle& operator=(const TimeoutHandle&) = delete;
        TimeoutHandle(TimeoutHandle&& other) noexcept
            : _tev(other._tev), _handle(other._handle)
        {
            other._handle = 0;
        }
        TimeoutHandle& operator=(TimeoutHandle&& other)
        {
            if (&_tev != &other._tev)
            {
                throw std::runtime_error("Cannot assign timeout handle from different Tev instances");
            }
            if (this != &other)
            {
                if (_handle != 0)
                {
                    _tev.ClearTimeout(_handle);
                }
                _handle = other._handle;
                other._handle = 0;
            }
            return *this;
        }

    private:
        Tev& _tev;
        Tev::TimeoutHandle _handle;
    };
}
