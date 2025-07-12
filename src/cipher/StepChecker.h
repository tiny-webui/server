#pragma once

#include <memory>
#include <stdexcept>
#include <functional>

namespace TUI::Cipher
{
    template<typename T>
    class StepChecker : public std::enable_shared_from_this<StepChecker<T>>
    {
    public:
        class Marker
        {
            friend class StepChecker<T>;
        public:
            Marker(const Marker&) = delete;
            Marker& operator=(const Marker&) = delete;
            Marker(Marker&&) = default;
            Marker& operator=(Marker&&) = default;

            ~Marker()
            {
                if (_uncaughtExceptionsOnEnter != std::uncaught_exceptions())
                {
                    if (_markWasted)
                    {
                        _markWasted();
                    }
                }
            }
        private:
            std::function<void()> _markWasted;
            int _uncaughtExceptionsOnEnter;

            Marker(std::function<void()> markWasted)
                : _markWasted(std::move(markWasted)),
                  _uncaughtExceptionsOnEnter(std::uncaught_exceptions())
            {
            }
        };

        static std::shared_ptr<StepChecker<T>> Create(T initialStep)
        {
            return std::shared_ptr<StepChecker<T>>(new StepChecker<T>(initialStep));
        }

        Marker CheckStep(T exceptedStep, T nextStep)
        {
            if (_wasted)
            {
                throw std::runtime_error("Procedure has been wasted");
            }
            if (_step != exceptedStep)
            {
                MarkWasted();
                throw std::runtime_error("Invalid step");
            }
            _step = nextStep;
            std::weak_ptr<StepChecker<T>> weakThis = this->shared_from_this();
            Marker marker([weakThis]() {
                auto sharedThis = weakThis.lock();
                if (sharedThis)
                {
                    sharedThis->MarkWasted();
                }
            });
            return std::move(marker);
        }

        T GetCurrentStep() const
        {
            if (_wasted)
            {
                throw std::runtime_error("Procedure has been wasted");
            }
            return _step;
        }
    private:
        T _step;
        bool _wasted{false};

        StepChecker(T initialStep)
            : _step(initialStep)
        {
        }

        void MarkWasted()
        {
            _wasted = true;
        }
    };
}

