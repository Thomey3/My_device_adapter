#pragma once

#include <atomic>
#include <vector>
#include <cstddef>

template<typename T, size_t N = 1024>
class LockFreeQueue
{
public:
	LockFreeQueue() {}
	LockFreeQueue(size_t s) : data_(s) {
		read_index_ = 0;
		write_index_ = 0;
	}
	~LockFreeQueue() {}

public:

	bool enqueue(T value) {
		size_t write_index = 0;
		Element* e = nullptr;
		do {
			write_index = write_index_.load(std::memory_order_relaxed);
			if (write_index >= (read_index_.load(std::memory_order_relaxed) + data_.size())) { // 等于时刚好满
				return false;
			}
			size_t index = write_index % N;
			e = &data_[index];
			if (e->full_.load(std::memory_order_relaxed)) {
				return false;
			}
		} while (!write_index_.compare_exchange_weak(write_index, write_index + 1), std::memory_order_release, std::memory_order_relaxed);

		e->data_ = std::move(value);
		e->full_.store(true, std::memory_order_release);
		return true;
	}

	bool dequeue(T& value) {
		size_t read_index = 0;
		Element* e = nullptr;
		do {
			read_index = read_index_.load(std::memory_order_relaxed);
			if (read_index >= write_index_.load(std::memory_order_relaxed)) {
				return false;
			}
			size_t index = read_index % N;
			e = &data_[index];
			if (!e->full_.load(std::memory_order_relaxed)) {
				return false;
			}
		} while (!read_index_.compare_exchange_weak(read_index, read_index + 1), std::memory_order_release, std::memory_order_relaxed);

		value = std::move(e->data_);
		e->full_.store(false, std::memory_order_release);
		return true;
	}
public:
	struct Element {
		std::atomic<bool> full_;
		T data_;
	};

private:
	std::vector<Element> data_;
	std::atomic<size_t> read_index_;
	std::atomic<size_t> write_index_;
};
