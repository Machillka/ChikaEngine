#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ChikaEngine::Framework
{
    // FIXME: 目前还没做线程安全
    class EventBus
    {
      public:
        using SubscriptionId = std::uint64_t;

        template <typename Event> SubscriptionId Subscribe(std::function<void(const Event&)> callback)
        {

            const SubscriptionId id = ++_nextSubscriptionId;
            _handlers[typeid(Event)].push_back(Handler{
                .id = id,
                .callback = [callback = std::move(callback)](const void* event) { callback(*static_cast<const Event*>(event)); },
            });
            return id;
        }

        template <typename Event> void Publish(const Event& event) const
        {
            const auto it = _handlers.find(typeid(Event));
            if (it == _handlers.end())
                return;

            const auto handlers = it->second;
            for (const auto& handler : handlers)
                handler.callback(&event);
        }

        void Unsubscribe(SubscriptionId id)
        {
            for (auto& [type, handlers] : _handlers)
            {
                std::erase_if(handlers, [id](const Handler& handler) { return handler.id == id; });
            }
        }

        void Clear()
        {
            _handlers.clear();
        }

      private:
        struct Handler
        {
            SubscriptionId id = 0;
            std::function<void(const void*)> callback;
        };

        SubscriptionId _nextSubscriptionId = 0;
        std::unordered_map<std::type_index, std::vector<Handler>> _handlers;
    };
} // namespace ChikaEngine::Framework
