#pragma once

#include <map>

template <typename T>
class EventEmitter {
public:
    void subscribe(T& m) {
        m_listeners[&m] = true;
    }
    void unsubscribe(T& m) {
        m_listeners[&m] = false;
    }

protected:
    template<typename E> void notify(E const& e) {
        for (auto m : m_listeners) {
            if (m.second)
                m.first->react(e);
        }
    }
    template<typename E> void notify(E & e) {
        for (auto m : m_listeners) {
            if (m.second)
                m.first->react(e);
        }
    }
private:
    std::map<T*, bool> m_listeners;

};
