#pragma once

#include <map>
#include <list>
#include <optional>

namespace TUI::Common
{
    template<typename K, typename V>
    class Cache
    {
    public:
        Cache(size_t maxSize)
            : _maxSize(maxSize)
        {
        }
    
        std::optional<V> TryGet(const K& key)
        {
            auto item = _cache.find(key);
            if (item == _cache.end())
            {
                return std::nullopt;
            }
            _order.splice(_order.begin(), _order, item->second.second);
            return item->second.first;
        }

        void Update(const K& key, const V& value)
        {
            auto item = _cache.find(key);
            if (item != _cache.end())
            {
                item->second.first = value;
                _order.splice(_order.begin(), _order, item->second.second);
                return;
            }
            if (_cache.size() >= _maxSize)
            {
                auto oldestKey = _order.back();
                _cache.erase(oldestKey);
                _order.pop_back();
            }
            _order.push_front(key);
            _cache[key] = {value, _order.begin()};
        }

    private:
        size_t _maxSize;
        std::map<K, std::pair<V, typename std::list<K>::iterator>> _cache{};
        std::list<K> _order{};
    };
}