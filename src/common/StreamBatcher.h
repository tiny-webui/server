#pragma once

#include <list>
#include <tev-cpp/Tev.h>
#include <js-style-co-routine/Promise.h>
#include <js-style-co-routine/AsyncGenerator.h>

namespace TUI::Common::StreamBatcher
{
    template<typename T>
    struct async_generator_traits;
    
    template<typename T, typename R>
    struct async_generator_traits<JS::AsyncGenerator<T, R>>
    {
        using value_type = T;
        using return_type = R;
    };

    template<typename AsyncGen>
    auto BatchStream(Tev& tev, AsyncGen source, uint64_t intervalMs)
        -> JS::AsyncGenerator<std::list<typename async_generator_traits<AsyncGen>::value_type>,
            typename async_generator_traits<AsyncGen>::return_type>
    {
        using T = async_generator_traits<AsyncGen>::value_type;

        Tev::Timeout intervalTimeout{};
        std::list<T> buffer{};
        std::optional<JS::Promise<bool>> awaitPromise{std::nullopt};
        std::optional<JS::Promise<std::optional<T>>> resultPromise{std::nullopt};
        bool ended{false};
        std::exception_ptr exception{nullptr};

        while (true)
        {
            awaitPromise = JS::Promise<bool>{};
            if (!resultPromise.has_value())
            {
                resultPromise = source.NextAsync();
                resultPromise->Then([&](std::optional<T> value) {
                    auto ref = std::move(resultPromise.value());
                    resultPromise.reset();
                    if (!value.has_value())
                    {
                        intervalTimeout.Clear();
                        ended = true;
                        if (awaitPromise.has_value())
                        {
                            awaitPromise->Resolve(false);
                        }
                        return;
                    }
                    if (intervalTimeout == nullptr)
                    {
                        intervalTimeout = tev.SetTimeout([&](){
                            /** Must call this to clear the handle */
                            intervalTimeout.Clear();
                            /** awaitPromise MUST has value here. */
                            awaitPromise->Resolve(true);
                        }, intervalMs);
                    }
                    buffer.push_back(std::move(value.value()));
                    awaitPromise->Resolve(false);
                });
                if (resultPromise.has_value())
                {
                    resultPromise->Catch([&](const std::exception &) {
                        auto ref = std::move(resultPromise.value());
                        resultPromise.reset();
                        intervalTimeout.Clear();
                        exception = std::current_exception();
                        awaitPromise->Resolve(false);
                    });
                }
            }
            auto isTimeout = co_await awaitPromise.value();
            awaitPromise.reset();
            if (!buffer.empty() && (isTimeout || exception || ended))
            {
                auto temp = std::move(buffer);
                buffer.clear();
                co_yield temp;
            }
            if (exception)
            {
                std::rethrow_exception(exception);
            }
            if (ended)
            {
                if constexpr (!std::is_void_v<typename async_generator_traits<AsyncGen>::return_type>)
                {
                    co_return source.GetReturnValue();
                }
                else
                {
                    co_return;
                }
            }
        }
    }
}
