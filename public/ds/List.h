//////////////////////////////////////////////////////////////////////////////////
// Copyright © Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Double Linked list
//////////////////////////////////////////////////////////////////////////////////

#pragma once

template <typename T>
class ListAbstract;

template <typename T>
struct ListNode
{
public:
	const T&	operator*() const { return value; }
	T&			operator*() { return value; }
	const T*	operator->() const { return &value; }
	T*			operator->() { return &value; }

	ListNode*	nextNode() const { return next; }
	ListNode*	prevNode() const { return prev; }
	const T&	getValue() const { return value; }
	T&			getValue() { return value; }

protected:
	T			value;
	ListNode*	prev{ nullptr };
	ListNode*	next{ nullptr };

	friend class ListAbstract<T>;
};

template <typename T>
class ListAbstract
{
public:
	using Node = ListNode<T>;

	int getCount() const { return m_count; }

	bool addFirst(const T& value)
	{
		Node* node = allocNode();
		node->value = value;
		insertNodeFirst(node);
		m_count++;
		return true;
	}

	bool addLast(const T& value)
	{
		Node* node = allocNode();
		node->value = value;
		insertNodeLast(node);
		m_count++;
		return true;
	}

	template<typename COMPARATOR>
	bool insertSorted(const T& value, COMPARATOR compareFunc)
	{
		Node* node = allocNode();
		node->value = value;

		Node* c = m_first;
		while (c != nullptr && compareFunc(c->value, value) <= 0)
		{
			c = c->m_next;
		}

		if (c)
			insertNodeBefore(c, node);
		else
			insertNodeLast(node);

		m_count++;
		return true;
	}

	const Node* front() const { return m_first; }
	Node* front() { return m_first; }

	const Node* back() const { return m_last; }
	Node* back() { return m_last; }

	Node* findFront(const T& value) const
	{
		Node* n = m_first;
		while (n != nullptr)
		{
			if (value == n->value)
				return n;
			n = n->next;
		}
		return nullptr;
	}

	Node* findBack(const T& value) const
	{
		Node* n = m_last;
		while (n != nullptr)
		{
			if (value == n->value)
				return n;
			n = n->prev;
		}
		return nullptr;
	}

	const T& getPrevWrap() const
	{
		return ((m_curr->prev != nullptr) ? m_curr->prev : m_last)->value;
	}

	const T& getNextWrap() const
	{
		return ((m_curr->next != nullptr) ? m_curr->next : m_first)->value;
	}

	T& getFirst() const
	{
		return m_first->value;
	}

	T& getLast() const
	{
		return m_last->value;
	}

	void clear()
	{
		if (m_del)
			freeNode(m_del);
		m_del = nullptr;

		Node* n = m_first;
		while (n != nullptr)
		{
			Node* next = n->next;
			freeNode(n);
			n = next;
		}

		m_first = m_last = m_curr = nullptr;
		m_count = 0;
	}

	void remove(Node* incidentNode)
	{
		if (!incidentNode)
			return;

		releaseNode(incidentNode);

		if (m_del)
			freeNode(m_del);

		m_del = incidentNode;
		m_count--;
	}

	void moveCurrentToTop()
	{
		if (m_curr != nullptr)
		{
			releaseNode(m_curr);
			insertNodeFirst(m_curr);
		}
	}

	bool insertBefore(const T& value, Node* incidentNode) 
	{
		Node* node = allocNode();
		node->value = value;
		insertNodeBefore(incidentNode, node);
		m_count++;
		return true;
	}

	bool insertAfter(const T& value, Node* incidentNode)
	{
		Node* node = allocNode();
		node->value = value;
		insertNodeAfter(incidentNode, node);
		m_count++;
		return true;
	}

	//------------------------------------------------
	// DEPRECATED API

	Node* goToFirst() { return m_curr = m_first; } // DEPRECATED
	Node* goToLast() { return m_curr = m_last; } // DEPRECATED
	Node* goToPrev() { return m_curr = m_curr->prevNode(); } // DEPRECATED
	Node* goToNext() { return m_curr = m_curr->nextNode(); } // DEPRECATED

	bool goToValue(const T& value) // DEPRECATED
	{
		m_curr = m_first;
		while (m_curr != nullptr)
		{
			if (value == m_curr->value) return true;
			m_curr = m_curr->next;
		}
		return false;
	}

	T& getCurrent() const // DEPRECATED
	{
		return m_curr->value;
	}

	Node* getCurrentNode() const // DEPRECATED
	{
		return m_curr;
	}

	void setCurrent(const T& value) // DEPRECATED
	{
		m_curr->value = value;
	}

	const T& getPrev() const // DEPRECATED
	{
		return m_curr->prev->value;
	}

	const T& getNext() const // DEPRECATED
	{
		return m_curr->next->value;
	}

	bool insertBeforeCurrent(const T& value) // DEPRECATED
	{
		Node* node = allocNode();
		node->value = value;
		insertNodeBefore(m_curr, node);
		m_count++;
		return true;
	}

	bool insertAfterCurrent(const T& value) // DEPRECATED
	{
		Node* node = allocNode();
		node->value = value;
		insertNodeAfter(m_curr, node);
		m_count++;
		return true;
	}

	bool removeCurrent() // DEPRECATED
	{
		if (m_curr != nullptr)
		{
			releaseNode(m_curr);

			if (m_del)
				freeNode(m_del);

			m_del = m_curr;

			m_count--;
		}
		return (m_curr != nullptr);
	}

protected:
	void insertNodeFirst(Node* node)
	{
		if (m_first != nullptr)
			m_first->prev = node;
		else
			m_last = node;

		node->next = m_first;
		node->prev = nullptr;

		m_first = node;
	}

	void insertNodeLast(Node* node)
	{
		if (m_last != nullptr)
			m_last->next = node;
		else
			m_first = node;

		node->prev = m_last;
		node->next = nullptr;

		m_last = node;
	}

	void insertNodeBefore(Node* at, Node* node)
	{
		Node* prev = at->prev;
		at->prev = node;
		if (prev)
			prev->next = node;
		else
			m_first = node;

		node->next = at;
		node->prev = prev;
	}

	void insertNodeAfter(Node* at, Node* node)
	{
		Node* next = at->next;
		at->next = node;
		if (next)
			next->prev = node;
		else
			m_last = node;

		node->prev = at;
		node->next = next;
	}

	void releaseNode(Node* node)
	{
		if (node->prev == nullptr)
			m_first = node->next;
		else
			node->prev->next = node->next;

		if (node->next == nullptr)
			m_last = node->prev;
		else
			node->next->prev = node->prev;

		node->next = nullptr;
		node->prev = nullptr;
	}

	virtual Node*	allocNode() = 0;
	virtual void	freeNode(Node* node) = 0;

	Node*	m_first{ nullptr };
	Node*	m_last{ nullptr };
	int		m_count{ 0 };

	Node*	m_curr{ nullptr };	// DEPRECATED
	Node*	m_del{ nullptr };	// DEPRECATED
};

//------------------------------------------------------

template <typename NODE>
struct ListAllocatorBase
{
	virtual			~ListAllocatorBase() {}

	virtual NODE*	alloc() = 0;
	virtual void	free(NODE* node) = 0;
};

template<typename NODE>
class DynamicListAllocator : public ListAllocatorBase<NODE>
{
public:
	DynamicListAllocator(const PPSourceLine& _sl)
		: m_sl(_sl)
	{
	}

	NODE* alloc()
	{
		return PPNewSL(m_sl) typename NODE;
	}

	void free(NODE* node)
	{
		delete node;
	}

private:
	const PPSourceLine m_sl;
};


template<typename NODE, int SIZE>
class FixedListAllocator : public ListAllocatorBase<NODE>
{
public:
	FixedListAllocator() 
	{
		for (int i = 0; i < SIZE; i++)
			m_nodeAlloc[i] = i;
	}

	NODE* alloc()
	{
		NODE* newNode = getNextNodeFromPool();

		ASSERT_MSG(newNode, "FixedListAllocator - No more free nodes in pool (%d)!", SIZE);

		new(newNode) NODE();
		return newNode;
	}

	void free(NODE* node)
	{
		const int idx = node - m_nodePool;
		ASSERT_MSG(idx >= 0 && idx < SIZE, "FixedListAllocator tried to remove Node that doesn't belong it's list!");

		node->~NODE();
		m_nodeAlloc[--m_nextNode] = idx;
	}

	NODE* getNextNodeFromPool()
	{
		if (m_nextNode < SIZE)
			return &m_nodePool[m_nodeAlloc[m_nextNode++]];

		return nullptr;
	}

	NODE m_nodePool[SIZE];
	ushort m_nodeAlloc[SIZE];
	ushort m_nextNode{ 0 };
};

template <typename T, typename ALLOCATOR>
class ListBase : public ListAbstract<T>
{
public:
	virtual ~ListBase()
	{
		clear();
	}

	ListBase()
	{
		static_assert(!std::is_same_v<ALLOCATOR, DynamicListAllocator<T>>, "PP_SL constructor is required to use");
	}

	ListBase(const PPSourceLine& sl) 
		: m_allocator(sl)
	{
	}

private:

	Node* allocNode()
	{
		return m_allocator.alloc();
	}

	void freeNode(Node* node)
	{
		return m_allocator.free(node);
	}

	ALLOCATOR m_allocator;
};

template<typename T>
using List = ListBase<T, DynamicListAllocator<ListNode<T>>>;

template<typename T, int SIZE>
using FixedList = ListBase<T, FixedListAllocator<ListNode<T>, SIZE>>;
