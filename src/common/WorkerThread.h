#pragma once

#include <thread>
#include <queue>
#include <mutex>
#include <any>
#include <functional>
#include <js-style-co-routine/Promise.h>
#include <tev-cpp/Tev.h>
#include "TevInjectionQueue.h"

namespace TUI::Common
{
    /**
     * caveat: This does not support non-copyable return types due to the use of std::any.
     */
    class WorkerThread
    {
    public:
        WorkerThread(Tev& tev)
            : _mainLoop(tev)
        {
            _taskQueue = TevInjectionQueue<std::pair<uint64_t, std::function<std::any()>>>::Create(
                _localLoop, std::bind(&WorkerThread::TaskQueueOnData, this, std::placeholders::_1));
            _closeQueue = TevInjectionQueue<bool>::Create(
                _localLoop, std::bind(&WorkerThread::CloseQueueOnData, this, std::placeholders::_1));
            _resultQueue = TevInjectionQueue<TaskResult>::Create(
                _mainLoop, std::bind(&WorkerThread::ResultQueueOnData, this, std::placeholders::_1));
            _workerThread = std::thread(std::bind(&WorkerThread::WorkerThreadFunc, this));
        }

        ~WorkerThread()
        {
            try
            {
                Close();
            }
            catch(...)
            {
                /** ignore */
            }
        }

        WorkerThread(const WorkerThread&) = delete;
        WorkerThread& operator=(const WorkerThread&) = delete;
        WorkerThread(WorkerThread&&) noexcept = delete;
        WorkerThread& operator=(WorkerThread&&) noexcept = delete;

        /**
         * @brief Execute a task asynchronously.
         * Make sure everything you do in the task is thread-safe.
         * 
         * @tparam Func 
         * @param task 
         * @return JS::Promise<decltype(task())> 
         */
        template<typename Func>
        auto ExecTaskAsync(Func&& task) -> JS::Promise<decltype(task())>
        {
            if (_closed)
            {
                throw std::runtime_error("WorkerThread closed");
            }

            using ReturnType = decltype(task());
            JS::Promise<ReturnType> promise;
            auto taskId = _taskIdSeed++;
            _resultCallbacks[taskId] = [=](TaskResult&& result) 
            {
                if (result.exception)
                {
                    try
                    {
                        std::rethrow_exception(result.exception);
                    }
                    catch (const std::exception& e)
                    {
                        promise.Reject(e.what());
                    }
                    return;
                }
                if constexpr (std::is_void_v<ReturnType>)
                {
                    promise.Resolve();
                    return;
                }
                else
                {
                    if (!result.result.has_value())
                    {
                        promise.Reject("Task returned no result");
                        return;    
                    }
                    try
                    {
                        promise.Resolve(std::any_cast<ReturnType>(std::move(result.result.value())));
                    }
                    catch (const std::bad_any_cast& e)
                    {
                        promise.Reject("Task returned wrong type: " + std::string(e.what()));
                    }
                }
            };
            _taskQueue->Inject(std::make_pair(taskId, [task = std::forward<Func>(task)]() -> std::any {
                if constexpr (std::is_void_v<ReturnType>)
                {
                    task();
                    return {};
                }
                else
                {
                    return std::any(task());
                }
            }));
            return promise;
        }

        /**
         * @brief Wait for the current task to finish and close the worker thread.
         * The current task and all pending tasks will fail with exception "WorkerThread closed".
         */
        void Close()
        {
            if (_closed)
            {
                return;
            }
            _closed = true;
            _resultQueue->Close();
            _closeQueue->Inject(true);
            if (_workerThread.joinable())
            {
                _workerThread.join();
            }
            /** Move stuffs out to void this going out of scope in the callbacks. */
            auto remainingResultCallbacks = std::move(_resultCallbacks);
            _resultCallbacks.clear();
            for (auto& [_, callback] : remainingResultCallbacks)
            {
                callback(TaskResult{0, std::nullopt, std::make_exception_ptr(std::runtime_error("WorkerThread closed"))});
            }
        }

    private:
        struct TaskResult
        {
            uint64_t taskId;
            std::optional<std::any> result{std::nullopt};
            std::exception_ptr exception{nullptr};
        };

        /**
         * Called in _localLoop
         * 
         * @param task 
         */
        void TaskQueueOnData(std::pair<uint64_t, std::function<std::any()>>&& task)
        {
            /** 
             * Whatever task is doing should not operate this Worker thread. 
             * Thus this will not go out of scope 
             */
            try
            {
                std::any result = task.second();
                TaskResult taskResult{task.first, std::move(result), nullptr};
                _resultQueue->Inject(std::move(taskResult));
            }
            catch(std::exception&)
            {
                TaskResult taskResult{task.first, std::nullopt, std::current_exception()};
                _resultQueue->Inject(std::move(taskResult));
            }
            catch(...)
            {
                TaskResult taskResult{task.first, std::nullopt, std::make_exception_ptr(std::runtime_error("Unknown exception"))};
                _resultQueue->Inject(std::move(taskResult));
            }
        }

        /**
         * Called in _mainLoop
         * 
         * @param result 
         */
        void ResultQueueOnData(TaskResult&& result)
        {
            auto it = _resultCallbacks.find(result.taskId);
            if (it == _resultCallbacks.end())
            {
                return;
            }
            auto callback = std::move(it->second);
            _resultCallbacks.erase(it);
            callback(std::move(result));
        }

        /**
         * Called in _localLoop
         */
        void CloseQueueOnData(bool&&)
        {
            /** NEVER reset the pointers. It is not a MT-safe flag. */
            _closeQueue->Close();
            _taskQueue->Close();
        }

        void WorkerThreadFunc()
        {
            _localLoop.MainLoop();
        }

        Tev& _mainLoop;
        Tev _localLoop{};
        std::thread _workerThread{};
        bool _closed{false};
        uint64_t _taskIdSeed{0};
        std::shared_ptr<TevInjectionQueue<TaskResult>> _resultQueue{nullptr};
        std::shared_ptr<TevInjectionQueue<std::pair<uint64_t, std::function<std::any()>>>> _taskQueue{nullptr};
        std::shared_ptr<TevInjectionQueue<bool>> _closeQueue{nullptr};
        std::unordered_map<uint64_t, std::function<void(TaskResult&&)>> _resultCallbacks{};
    };
}
