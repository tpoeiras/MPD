/*
 * Copyright (C) 2003-2014 Max Kellermann <max@duempel.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FOREIGN_FIFO_BUFFER_HXX
#define FOREIGN_FIFO_BUFFER_HXX

#include "WritableBuffer.hxx"

#include <utility>
#include <algorithm>

#include <cstddef>

#include <assert.h>

/**
 * A first-in-first-out buffer: you can append data at the end, and
 * read data from the beginning.  This class automatically shifts the
 * buffer as needed.  It is not thread safe.
 *
 * This class does not manage buffer memory.  It will not allocate or
 * free any memory, it only manages the contents of an existing buffer
 * given to the constructor.
 */
template<typename T>
class ForeignFifoBuffer {
public:
	typedef size_t size_type;
	typedef WritableBuffer<T> Range;
	typedef typename Range::pointer_type pointer_type;
	typedef typename Range::const_pointer_type const_pointer_type;

protected:
	size_type head, tail, capacity;
	T *data;

public:
	explicit constexpr ForeignFifoBuffer(std::nullptr_t n)
		:head(0), tail(0), capacity(0), data(n) {}

	constexpr ForeignFifoBuffer(T *_data, size_type _capacity)
		:head(0), tail(0), capacity(_capacity), data(_data) {}

	ForeignFifoBuffer(ForeignFifoBuffer &&src)
		:head(src.head), tail(src.tail),
		 capacity(src.capacity), data(src.data) {
		src.SetNull();
	}

	ForeignFifoBuffer &operator=(ForeignFifoBuffer &&src) {
		head = src.head;
		tail = src.tail;
		capacity = src.capacity;
		data = src.data;
		src.SetNull();
		return *this;
	}

	constexpr bool IsNull() const {
		return data == nullptr;
	}

	constexpr bool IsDefined() const {
		return !IsNull();
	}

	T *GetBuffer() {
		return data;
	}

	constexpr size_type GetCapacity() const {
		return capacity;
	}

	void SetNull() {
		head = tail = 0;
		capacity = 0;
		data = nullptr;
	}

	void SetBuffer(T *_data, size_type _capacity) {
		assert(_data != nullptr);
		assert(_capacity > 0);

		head = tail = 0;
		capacity = _capacity;
		data = _data;
	}

	void MoveBuffer(T *new_data, size_type new_capacity) {
		assert(new_capacity >= tail - head);

		std::move(data + head, data + tail, new_data);
		data = new_data;
		capacity = new_capacity;
		tail -= head;
		head = 0;
	}

	void Clear() {
		head = tail = 0;
	}

	constexpr bool IsEmpty() const {
		return head == tail;
	}

	constexpr bool IsFull() const {
		return head == 0 && tail == capacity;
	}

	/**
	 * Prepares writing.  Returns a buffer range which may be written.
	 * When you are finished, call append().
	 */
	Range Write() {
		Shift();
		return Range(data + tail, capacity - tail);
	}

	bool WantWrite(size_type n) {
		if (tail + n <= capacity)
			/* enough space after the tail */
			return true;

		const size_type in_use = tail - head;
		const size_type required_capacity = in_use + n;
		if (required_capacity > capacity)
			return false;

		Shift();
		assert(tail + n <= capacity);
		return true;
	}

	/**
	 * Expands the tail of the buffer, after data has been written to
	 * the buffer returned by write().
	 */
	void Append(size_type n) {
		assert(tail <= capacity);
		assert(n <= capacity);
		assert(tail + n <= capacity);

		tail += n;
	}

	constexpr size_type GetAvailable() const {
		return tail - head;
	}

	/**
	 * Return a buffer range which may be read.  The buffer pointer is
	 * writable, to allow modifications while parsing.
	 */
	constexpr Range Read() const {
		return Range(data + head, tail - head);
	}

	/**
	 * Marks a chunk as consumed.
	 */
	void Consume(size_type n) {
		assert(tail <= capacity);
		assert(head <= tail);
		assert(n <= tail);
		assert(head + n <= tail);

		head += n;
	}

	size_type Read(pointer_type p, size_type n) {
		auto range = Read();
		if (n > range.size)
			n = range.size;
		std::copy_n(range.data, n, p);
		Consume(n);
		return n;
	}

protected:
	void Shift() {
		if (head == 0)
			return;

		assert(head <= capacity);
		assert(tail <= capacity);
		assert(tail >= head);

		std::move(data + head, data + tail, data);

		tail -= head;
		head = 0;
	}
};

#endif
