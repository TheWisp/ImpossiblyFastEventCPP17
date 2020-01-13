#pragma once

namespace ifevec
{
	template<typename F> struct Event;

	struct EventBase;
	struct ListenerBase
	{
		EventBase* m_Event = nullptr;
		size_t m_idx;
	};

	struct EventBase
	{
		struct Callback
		{
			void* object;
			void* func;
		};

		union
		{
			struct
			{
				Callback* m_Callbacks;
				ListenerBase** m_Listeners;
				size_t m_Size : 8 * sizeof size_t - 2;
				size_t m_Calling : 1;//represents if we are in the middle of calling back, no matter recursive or not.
				size_t m_Dirty : 1;
			};

			struct
			{
				Callback m_SingleCb;
				ListenerBase* m_SingleListener;
			};
		};

		size_t m_Capacity : 8 * sizeof size_t - 1;
		size_t m_Multi : 1; //flag, 0 => holding single listener

		EventBase() : m_SingleCb(), m_SingleListener(), m_Capacity(), m_Multi()
		{
		}

		~EventBase()
		{
			if (m_Multi)
			{
				for (ListenerBase** l = m_Listeners, **end = m_Listeners + m_Size; l < end; ++l)
					if (*l) (*l)->m_Event = nullptr;
				free(m_Listeners);
				free(m_Callbacks);
			}
			else
			{
				if (m_SingleListener)
					m_SingleListener->m_Event = nullptr;
			}
		}

		void add(void* object, void* func, ListenerBase* pListener)
		{
			if (!m_Multi)
			{
				if (!m_SingleListener)
				{
					m_SingleCb.object = object;
					m_SingleCb.func = func;
					m_SingleListener = pListener;
					pListener->m_idx = 0;
				}
				else
				{
					m_Multi = 1;
					m_Capacity = 2;//initially 2 slots
					Callback* callbacks = (Callback*)malloc(sizeof Callback * m_Capacity);
					callbacks[0] = m_SingleCb;
					callbacks[1].object = object;
					callbacks[1].func = func;
					ListenerBase** listeners = (ListenerBase * *)malloc(sizeof(void*) * m_Capacity);
					listeners[0] = m_SingleListener;
					listeners[1] = pListener;
					m_Callbacks = callbacks;
					m_Listeners = listeners;
					m_Size = 2;
					m_Calling = 0;
					m_Dirty = 0;
					pListener->m_idx = 1;
				}
			}
			else
			{
				if (m_Size == m_Capacity)
				{
					//grow and copy
					m_Capacity = (m_Capacity * 3) / 2; //grow by 1.5    2->3->4->6->9...
					m_Callbacks = (Callback*)realloc(m_Callbacks, sizeof Callback * m_Capacity);
					m_Listeners = (ListenerBase * *)realloc(m_Listeners, sizeof(void*) * m_Capacity);
				}
				m_Callbacks[m_Size].object = object;
				m_Callbacks[m_Size].func = func;
				m_Listeners[m_Size] = pListener;
				pListener->m_idx = m_Size++;
			}
		}

		void remove(size_t idx)
		{
			if (m_Multi)
			{
				m_Callbacks[idx].object = m_Callbacks[idx].func = nullptr;
				m_Listeners[idx] = nullptr;
				m_Dirty = 1;
			}
			else
			{
				m_SingleCb.object = nullptr;
				m_SingleCb.func = nullptr;
				m_SingleListener = nullptr;
			}
		}

		void replace(size_t idx, void* object, ListenerBase* listener)
		{
			if (m_Multi)
			{
				m_Callbacks[idx].object = object;
				m_Listeners[idx] = listener;
			}
			else
			{
				m_SingleCb.object = object;
				m_SingleListener = listener;
			}
		}

	};

	template<typename R, typename ... A>
	struct Event<R(A...)> : EventBase
	{
		template<auto, auto> friend struct Listener;
		using CallbackFuncType = R(void*, A...);

		template<typename ... ActualArgsT>
		void operator()(ActualArgsT&& ... args)
		{
			if (m_Multi)
			{
				bool isRecursion = m_Calling;
				if (!m_Calling) m_Calling = 1;

				//TODO: using index is necessary because of potential reallocation during callback. Can we make it faster? (yet another flag?)
				//TODO: alternatively, simply use another buffer for newly added ones during callback?
				for (size_t i = 0; i < m_Size; ++i)
				{
					auto& cb = m_Callbacks[i];
					if (cb.func)
						static_cast<CallbackFuncType*>(cb.func)(cb.object, std::forward<ActualArgsT>(args)...);
				}

				if (!isRecursion)
				{
					m_Calling = 0;

					if (m_Dirty)
					{
						m_Dirty = 0;
						//remove all empty slots while patching the stored index in the listener
						size_t sz = 0;
						for (size_t i = 0; i < m_Size; ++i)
						{
							if (m_Listeners[i]) {
								m_Listeners[sz] = m_Listeners[i];
								m_Callbacks[sz] = m_Callbacks[i];
								m_Listeners[sz]->m_idx = sz;
								++sz;
							}
						}
						m_Size = sz;
					}
				}
			}
			else
			{
				if (m_SingleCb.func)
					static_cast<CallbackFuncType*>(m_SingleCb.func)(m_SingleCb.object, std::forward<ActualArgsT>(args)...);
			}
		}
	};

	static_assert(sizeof Event<void()> == 4 * sizeof size_t);

	template<auto EventPtr, auto CallbackPtr> struct Listener;

	template<
		class EventClass, typename... EventFuncArgsT, Event<void(EventFuncArgsT...)> EventClass::* EventPtr,
		class CallbackClass, typename CallbackFuncType, CallbackFuncType CallbackClass::* CallbackPtr >
		struct Listener<EventPtr, CallbackPtr> : public ListenerBase
	{
		using EventType = Event<void(EventFuncArgsT...)>;

		CallbackClass* m_object = nullptr;

		Listener() = default;

		Listener(EventClass* eventSender, CallbackClass* eventReceiver)
		{
			connect(eventSender, eventReceiver);
		}

		Listener(const Listener&) = delete;
		Listener(Listener&& other) : ListenerBase(other)
		{
			//fix the object
			if (other.m_object)
				m_object = (CallbackClass*)((size_t)this - ((size_t)& other - (size_t)other.m_object));

			//fix the links
			if (m_Event)
				m_Event->replace(m_idx, m_object, this);

			other.m_object = nullptr;
			other.m_Event = nullptr;
		}

		void connect(EventClass* eventSender, CallbackClass* eventReceiver)
		{
			m_object = eventReceiver;
			m_Event = &(eventSender->*EventPtr);
			m_Event->add(eventReceiver, +[](void* obj, EventFuncArgsT ... args) {(static_cast<CallbackClass*>(obj)->*CallbackPtr)(args...); }, this);
		}

		void disconnect()
		{
			if (m_Event) m_Event->remove(m_idx);
		}

		~Listener()
		{
			disconnect();
		}
	};
}
