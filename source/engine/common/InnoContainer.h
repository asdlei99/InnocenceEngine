#pragma once
#include "../common/stl14.h"
#include "../common/stl17.h"

#include "../system/Core/InnoAllocator.h"

template <typename T>
class ThreadSafeQueue
{
public:
	~ThreadSafeQueue(void)
	{
		invalidate();
	}

	bool tryPop(T& out)
	{
		std::lock_guard<std::shared_mutex> lock{ m_mutex };
		if (m_queue.empty() || !m_valid)
		{
			return false;
		}
		out = std::move(m_queue.front());
		m_queue.pop();
		return true;
	}

	bool waitPop(T& out)
	{
		std::unique_lock<std::shared_mutex> lock{ m_mutex };
		m_condition.wait(lock, [this]()
		{
			return !m_queue.empty() || !m_valid;
		});

		if (!m_valid)
		{
			return false;
		}
		out = std::move(m_queue.front());
		m_queue.pop();
		return true;
	}

	void push(T value)
	{
		std::lock_guard<std::shared_mutex> lock{ m_mutex };
		m_queue.push(std::move(value));
		m_condition.notify_one();
	}

	bool empty(void) const
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_queue.empty();
	}

	void clear(void)
	{
		std::lock_guard<std::shared_mutex> lock{ m_mutex };
		while (!m_queue.empty())
		{
			m_queue.pop();
		}
		m_condition.notify_all();
	}

	bool isValid(void) const
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_valid;
	}

	void invalidate(void)
	{
		std::lock_guard<std::shared_mutex> lock{ m_mutex };
		m_valid = false;
		m_condition.notify_all();
	}

	size_t size(void)
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_queue.size();
	}

private:
	std::atomic_bool m_valid{ true };
	mutable std::shared_mutex m_mutex;
	std::queue<T> m_queue;
	std::condition_variable_any m_condition;
};

template <typename T>
class ThreadSafeVector
{
public:
	~ThreadSafeVector(void)
	{
		invalidate();
	}

	T& operator[](std::size_t pos)
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_vector[pos];
	}

	const T& operator[](std::size_t pos) const
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_vector[pos];
	}

	void push_back(T&& value)
	{
		std::lock_guard<std::shared_mutex> lock{ m_mutex };
		m_vector.push_back(value);
		m_condition.notify_one();
	}

	template <class... T>
	void emplace_back(T&&... values)
	{
		std::lock_guard<std::shared_mutex> lock{ m_mutex };
		m_vector.emplace_back(values ...);
		m_condition.notify_one();
	}

	auto empty(void) const
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_vector.empty();
	}

	void clear(void)
	{
		std::lock_guard<std::shared_mutex> lock{ m_mutex };
		m_vector.clear();
		m_condition.notify_all();
	}

	auto begin(void)
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_vector.begin();
	}

	auto end(void)
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_vector.end();
	}

	bool isValid(void) const
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_valid;
	}

	void invalidate(void)
	{
		std::lock_guard<std::shared_mutex> lock{ m_mutex };
		m_valid = false;
		m_condition.notify_all();
	}

	auto size(void)
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_vector.size();
	}

	std::vector<T>& getRawData(void)
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_vector;
	}

	void setRawData(std::vector<T>&& values)
	{
		std::lock_guard<std::shared_mutex> lock{ m_mutex };
		m_vector = values;
	}

private:
	std::atomic_bool m_valid{ true };
	mutable std::shared_mutex m_mutex;
	std::vector<T> m_vector;
	std::condition_variable_any m_condition;
};

template <typename Key, typename T>
class ThreadSafeUnorderedMap
{
public:
	~ThreadSafeUnorderedMap(void)
	{
		invalidate();
	}

	void emplace(Key key, T value)
	{
		std::lock_guard<std::shared_mutex> lock{ m_mutex };
		m_unordered_map.emplace(key, value);
		m_condition.notify_one();
	}

	void emplace(std::pair<Key, T> value)
	{
		std::lock_guard<std::shared_mutex> lock{ m_mutex };
		m_unordered_map.emplace(value);
		m_condition.notify_one();
	}

	auto begin(void)
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_unordered_map.begin();
	}

	auto end(void)
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_unordered_map.end();
	}

	auto find(const Key& key)
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_unordered_map.find(key);
	}

	auto find(const Key& key) const
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_unordered_map.find(key);
	}

	auto erase(const Key& key)
	{
		std::lock_guard<std::shared_mutex> lock{ m_mutex };
		return m_unordered_map.erase(key);
	}

	auto clear(void)
	{
		std::lock_guard<std::shared_mutex> lock{ m_mutex };
		return m_unordered_map.clear();
	}

	void invalidate(void)
	{
		std::lock_guard<std::shared_mutex> lock{ m_mutex };
		m_valid = false;
		m_condition.notify_all();
	}

	auto size(void)
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_unordered_map.size();
	}

	std::unordered_map<Key, T>& getRawData(void)
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };
		return m_unordered_map;
	}

private:
	std::atomic_bool m_valid{ true };
	mutable std::shared_mutex m_mutex;
	std::unordered_map<Key, T> m_unordered_map;
	std::condition_variable_any m_condition;
};

template<class _Ty>
class FixedSizeArray
{
public:
	explicit FixedSizeArray(std::size_t size) { m_vector.reserve(size); };

	_Ty& operator[](std::size_t pos)
	{
		return m_vector[pos];
	}

	const _Ty& operator[](std::size_t pos) const
	{
		return m_vector[pos];
	}

	void emplace_back(_Ty&& value)
	{
		m_vector.emplace_back(value);
	}

	auto empty(void) const
	{
		return m_vector.empty();
	}

	void clear(void)
	{
		m_vector.clear();
	}

	auto begin(void)
	{
		return m_vector.begin();
	}

	auto end(void)
	{
		return m_vector.end();
	}

	auto size(void)
	{
		return m_vector.size();
	}

private:
	std::vector<_Ty, innoAllocator<_Ty>> m_vector;
};