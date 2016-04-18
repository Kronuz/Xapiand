/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "test_queue.h"

#include "../src/queue.h"
#include "../src/log.h"

#define RETURN(x) { Log::finish(); return x; }

using namespace queue;


int test_unique() {
	Queue<std::unique_ptr<std::string>> messages_queue;
	messages_queue.push(std::make_unique<std::string>("This is a unique data"));
	if (messages_queue.size() != 1) {
		L_ERR(nullptr, "push is not working with unique_ptr.");
		RETURN(1);
	}

	std::unique_ptr<std::string> msg;
	if (!messages_queue.pop(msg)) {
		L_ERR(nullptr, "pop is not working with unique_ptr.");
		RETURN(1);
	}

	if (messages_queue.size() != 0) {
		L_ERR(nullptr, "size is not working with unique_ptr.");
		RETURN(1);
	}

	if (*msg != "This is a unique data") {
		L_ERR(nullptr, "pop is changing memory with unique_ptr.");
		RETURN(1);
	}

	RETURN(0);
}


int test_shared() {
	Queue<std::shared_ptr<std::string>> messages_queue;
	messages_queue.push(std::make_shared<std::string>("This is a shared data"));
	if (messages_queue.size() != 1) {
		L_ERR(nullptr, "push is not working with shared_ptr.");
		 RETURN(1);
	}

	std::shared_ptr<std::string> shared = messages_queue.front();
	if (messages_queue.size() != 1) {
		L_ERR(nullptr, "front is not working with shared_ptr.");
		 RETURN(1);
	}

	if (shared.use_count() != 2) {
		L_ERR(nullptr, "Lose memory with shared_ptr.");
		 RETURN(1);
	}

	std::shared_ptr<std::string> msg;
	if (!messages_queue.pop(msg)) {
		L_ERR(nullptr, "pop is not working with shared_ptr.");
		 RETURN(1);
	}

	if (messages_queue.size() != 0)  {
		L_ERR(nullptr, "size is not working with shared_ptr.");
		 RETURN(1);
	}

	if (*msg != "This is a shared data")  {
		L_ERR(nullptr, "pop is changing memory with shared_ptr.");
		 RETURN(1);
	}

	 RETURN(0);
}


int test_queue() {
	Queue<int> q;
	int val = 1;

	q.push(val);
	q.push(2);
	q.push(3);
	val = 4;
	q.push(val);

	if (q.size() != 4) {
		L_ERR(nullptr, "push is not working with int.");
		RETURN(1);
	}

	int i1, i2, i3, i4;

	if (!q.pop(i1, 0) || !q.pop(i2, 0) || !q.pop(i3, 0) || !q.pop(i4, 0)) {
		L_ERR(nullptr, "pop is not working with int.");
		RETURN(1);
	}

	if (i1 != 1 || i2 != 2 || i3 != 3 || i4 != 4) {
		L_ERR(nullptr, "pop is changing memory with int.");
		RETURN(1);
	}

	RETURN(0);
}


int test_queue_set() {
	QueueSet<int> q;
	int val = 1;

	q.push(val);
	q.push(2);
	val = 3;
	q.push(val);
	q.push(4);
	q.push(1);  // renew by default, doesn't insert a new item

	if (q.size() != 4) {
		L_ERR(nullptr, "QueueSet::push is not working.");
		 RETURN(1);
	}

	int i1, i2, i3, i4, i5 = 789;

	if (!q.pop(i1, 0) || !q.pop(i2, 0) || !q.pop(i3, 0) || !q.pop(i4, 0) || q.pop(i5, 0)) {
		L_ERR(nullptr, "QueueSet::pop is not working.");
		 RETURN(1);
	}

	if (i1 != 2 || i2 != 3 || i3 != 4 || i4 != 1 || i5 != 789) {
		L_ERR(nullptr, "QueueSet::pop is changing memory.");
		 RETURN(1);
	}

	 RETURN(0);
}


int test_queue_set_on_dup() {
	QueueSet<int> q;
	int val = 1;

	q.push(val);
	q.push(2);
	q.push(3);
	q.push(4);
	q.push(1, [](int&){ return DupAction::leave; });  // doesn't touch the item
	val = 2;
	q.push(val, [](int&){ return DupAction::update; });  // updates the item inplace
	q.push(3, [](int&){ return DupAction::renew; });  // renews the item

	if (q.size() != 4) {
		L_ERR(nullptr, "QueueSet::push with set_on_dup is not working.");
		 RETURN(1);
	}

	int i1, i2, i3, i4, i5 = 789;

	if (!q.pop(i1, 0) || !q.pop(i2, 0) || !q.pop(i3, 0) || !q.pop(i4, 0) || q.pop(i5, 0)) {
		L_ERR(nullptr, "QueueSet::pop with set_on_dup is not working.");
		 RETURN(1);
	}

	L_DEBUG(nullptr, "%d %d %d %d %d", i1, i2, i3, i4, i5);

	if (i1 != 1 || i2 != 2 || i3 != 4 || i4 != 3 || i5 != 789) {
		L_ERR(nullptr, "QueueSet::pop with set_on_dup is changing memory.");
		 RETURN(1);
	}

	 RETURN(0);
}


int test_queue_constructor() {
	std::pair<int, Queue<int>> foo = std::make_pair(1, Queue<int>());
	foo.second.push(1);
	foo.second.push(2);
	foo.second.push(3);

	int i1, i2, i3;

	if (!foo.second.pop(i1, 0) || !foo.second.pop(i2, 0) || !foo.second.pop(i3, 0) || foo.second.size() != 0) {
		L_ERR(nullptr, "QueueSet move constructor is not working.");
		 RETURN(1);
	}

	 RETURN(0);
}
