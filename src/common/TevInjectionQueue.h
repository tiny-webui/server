#pragma once

#include <functional>
#include <queue>
#include <mutex>
#include <memory>
#include <optional>
#include <string.h>
#include <sys/eventfd.h>
#include <tev-cpp/Tev.h>
#include "UniqueTypes.h"

namespace TUI::Common
{
    /**
     * @brief Injects data into the event loop in a thread-safe manner.
     * 
     * @tparam T 
     */
    template<typename T>
    class TevInjectionQueue : public std::enable_shared_from_this<TevInjectionQueue<T>>
    {
    public:
        static std::shared_ptr<TevInjectionQueue<T>> Create(
            Tev& tev, std::function<void(T&&)> onData, std::function<void()> onError = nullptr)
        {
            return std::shared_ptr<TevInjectionQueue<T>>(
                new TevInjectionQueue<T>(tev, std::move(onData), std::move(onError)));
        }

        TevInjectionQueue(const TevInjectionQueue&) = delete;
        TevInjectionQueue& operator=(const TevInjectionQueue&) = delete;
        TevInjectionQueue(TevInjectionQueue&&) noexcept = delete;
        TevInjectionQueue& operator=(TevInjectionQueue&&) noexcept = delete;
        ~TevInjectionQueue() = default;

        /**
         * @brief Called in any thread to enqueue data into the event loop.
         * 
         * @param data 
         */
        void Inject(T&& data)
        {
            if (_eventFd < 0)
            {
                throw std::runtime_error("TevInjectionQueue is closed");
            }
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _queue.push(std::move(data));
            }
            if (eventfd_write(_eventFd, 1) != 0)
            {
                throw std::runtime_error("Failed to write to eventfd: " + std::string(strerror(errno)));
            }
        }
        void Inject(const T& data)
        {
            Inject(T(data));
        }

        /**
         * @brief Disconnect from the event loop.
         * Should ONLY be called from the event loop.
         */
        void Close()
        {
            CloseInternal();
        }
    private:
        TevInjectionQueue(Tev& tev, std::function<void(T&&)> onData, std::function<void()> onError)
            : _tev(tev), _onData(std::move(onData)), _onError(std::move(onError))
        {
            _eventFd = Unique::Fd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
            if (_eventFd < 0)
            {
                throw std::runtime_error("Failed to create eventfd: " + std::string(strerror(errno)));
            }
            _readHandler = _tev.SetReadHandler(_eventFd, std::bind(&TevInjectionQueue<T>::ReadHandler, this));
        }

        void ReadHandler()
        {
            /** Avoid this going out of scope in one of the user's callbacks */
            auto self = this->shared_from_this();
            eventfd_t v;
            if (eventfd_read(_eventFd, &v) != 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    return;
                }
                CloseInternal(true);
                return;
            }
            while(true)
            {
                if (_eventFd < 0)
                {
                    /** 
                     * The queue is closed, should not process more data
                     * This can happen in the previous callback.
                     */
                    break;
                }
                std::optional<T> data;
                {
                    std::lock_guard<std::mutex> lock(_mutex);
                    if (_queue.empty())
                    {
                        break;
                    }
                    data = std::move(_queue.front());
                    _queue.pop();
                }
                if (!data.has_value())
                {
                    break;
                }
                try
                {
                    _onData(std::move(data.value()));
                }
                catch(...)
                {
                    /** Ignored */
                }
            }
        }

        void CloseInternal(bool isError = false)
        {
            if (_readHandler != nullptr)
            {
                _readHandler.Clear();
            }
            if (_eventFd >= 0)
            {
                _eventFd = Unique::Fd(-1);
            }
            {
                std::lock_guard<std::mutex> lock(_mutex);
                while (!_queue.empty())
                {
                    _queue.pop();
                }
            }
            if (isError && _onError)
            {
                _onError();
            }
        }

        Tev& _tev;
        std::function<void(T&&)> _onData;
        std::function<void()> _onError;
        std::queue<T> _queue{};
        std::mutex _mutex{};
        Unique::Fd _eventFd{-1};
        Tev::FdHandler _readHandler{};
    };
}
