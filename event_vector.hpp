#pragma once
#include <vector>

namespace ifevec
{
	template<typename F> struct Event;

	struct ListenerBase
	{
		void* m_Event = nullptr;
		size_t m_idx;
	};

	template<typename R, typename ... A>
	struct Event<R(A...)>
	{
	private:
		template<auto, auto> friend struct Listener;
		using CallbackFuncType = R(void*, A...);

		struct Callback
		{
			void* object;
			CallbackFuncType* func;

			Callback(void* object, CallbackFuncType* func)
				: object(object)
				, func(func)
			{}
		};

		//Caches the callback of head as a small-object optimization for single callback
		Callback first = { nullptr, nullptr };

		std::vector<Callback> m_Callbacks;
		std::vector<ListenerBase*> m_Listeners;

		void add(void* object, CallbackFuncType* func, ListenerBase* pListener)
		{
			m_Callbacks.emplace_back(object, func);
			m_Listeners.emplace_back(pListener);
			if (m_Callbacks.size() == 1) first = m_Callbacks[0];
			else first = { nullptr, nullptr };
			pListener->m_idx = m_Callbacks.size() - 1;
		}

		void remove(size_t idx)
		{
			if (idx != m_Callbacks.size() - 1)
			{
				std::swap(m_Callbacks[idx], m_Callbacks.back());
				std::swap(m_Listeners[idx], m_Listeners.back());
				m_Listeners[idx]->m_idx = idx;
			}
			m_Callbacks.pop_back();
			m_Listeners.pop_back();
			if (m_Callbacks.size() == 1) first = m_Callbacks[0];
			else first = { nullptr, nullptr };
		}

	public:

		~Event()
		{
			for (ListenerBase* l : m_Listeners)
				l->m_Event = nullptr;
		}

		template<typename ... ActualArgsT>
		void operator()(ActualArgsT&&... args)
		{
			if (first.func) {
				first.func(first.object, std::forward<ActualArgsT>(args)...);
			}
			else if(!m_Callbacks.empty()) //we have at least two
			{
				for (auto& cb : m_Callbacks)
					cb.func(cb.object, std::forward<ActualArgsT>(args)...);
			}
		}
	};

	template<auto EventPtr, auto CallbackPtr> struct Listener;

	template< 
		class EventClass, typename... EventFuncArgsT, Event<void(EventFuncArgsT...)> EventClass:: * EventPtr,
		class CallbackClass, typename CallbackFuncType, CallbackFuncType CallbackClass:: * CallbackPtr >
	struct Listener<EventPtr, CallbackPtr> : public ListenerBase
	{
		using EventType = Event<void(EventFuncArgsT...)>;

		Listener() = default;

		Listener(EventClass* eventSender, CallbackClass* eventReceiver)
		{
			connect(eventSender, eventReceiver);
		}

		void connect(EventClass* eventSender, CallbackClass* eventReceiver)
		{
			m_Event = &(eventSender->*EventPtr);
			static_cast<EventType*>(m_Event)->add(eventReceiver, call, this);
		}

		void disconnect()
		{
			if (m_Event) static_cast<EventType*>(m_Event)->remove(m_idx);
		}

		~Listener()
		{
			disconnect();
		}

		static void call(void* obj, EventFuncArgsT ... args)
		{
			(static_cast<CallbackClass*>(obj)->*CallbackPtr)(args...);
		}
	};
}

