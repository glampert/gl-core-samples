
// ================================================================================================
// -*- C++ -*-
// File: linked_list.hpp
// Author: Guilherme R. Lampert
// Created on: 06/07/16
// Brief: Simple intrusive doubly-linked list template.
// ================================================================================================

#ifndef LINKED_LIST_HPP
#define LINKED_LIST_HPP

#include <cassert>

// ========================================================
// template class LinkedList<T>:
// ========================================================

//
// Type T must define the following public members:
//
// struct T
// {
//     T * next;
//     T * prev;
//     bool linked() const { return prev != nullptr && next != nullptr; }
// };
//
// Some of the member methods in this class are named as the
// std::list<T> equivalents to have some degree of compatibility.
//
// Note: Items can only be members of one list at a time!
//
template<class T>
class LinkedList final
{
public:

    LinkedList() = default;

    //
    // push_front/back:
    // Append at the head or tail of the list.
    // Both are constant time. Node must not be null!
    //
    void push_front(T * node)
    {
        assert(node != nullptr);
        assert(!node->linked()); // A node can only be a member of one list at a time!

        if (!empty())
        {
            // head->prev points the tail, and vice-versa:
            T * tail = head->prev;
            node->next = head;
            head->prev = node;
            node->prev = tail;
            head = node;
        }
        else // Empty list, first insertion:
        {
            head = node;
            head->prev = head;
            head->next = head;
        }
        ++count;
    }
    void push_back(T * node)
    {
        assert(node != nullptr);
        assert(!node->linked()); // A node can only be a member of one list at a time!

        if (!empty())
        {
            // head->prev points the tail, and vice-versa:
            T * tail = head->prev;
            node->prev = tail;
            tail->next = node;
            node->next = head;
            head->prev = node;
        }
        else // Empty list, first insertion:
        {
            head = node;
            head->prev = head;
            head->next = head;
        }
        ++count;
    }

    //
    // pop_front/back:
    // Removes one node from head or tail of the list, without destroying the object.
    // Returns the removed node or null if the list is already empty. Both are constant time.
    //
    T * pop_front()
    {
        if (empty())
        {
            return nullptr;
        }

        T * removedNode = head;
        T * tail = head->prev;
        head = head->next;
        head->prev = tail;
        --count;

        removedNode->prev = nullptr;
        removedNode->next = nullptr;
        return removedNode;
    }
    T * pop_back()
    {
        if (empty())
        {
            return nullptr;
        }

        T * removedNode = head->prev;
        T * tail = head->prev;
        head->prev = tail->prev;
        tail->prev->next = head;
        --count;

        removedNode->prev = nullptr;
        removedNode->next = nullptr;
        return removedNode;
    }

    //
    // Unlink nodes from anywhere in the list:
    // (node is *not* destroyed nor deleted)
    //
    void remove(T * node)
    {
        // Assumes the node is linked to THIS LIST.
        assert(node != nullptr);
        assert(node->linked());
        assert(!empty());

        if (node == head) // Head
        {
            pop_front();
        }
        else if (node == head->prev) // Tail
        {
            pop_back();
        }
        else // Middle
        {
            T * nodePrev = node->prev;
            T * nodeNext = node->next;
            nodePrev->next = nodeNext;
            nodeNext->prev = nodePrev;
            node->prev = nullptr;
            node->next = nullptr;
            --count;
        }
    }
    void clear()
    {
        T * node = head;
        while (count--)
        {
            T * tmp = node;
            node = node->next;

            assert(tmp != nullptr);
            tmp->prev = nullptr;
            tmp->next = nullptr;
        }
        head = nullptr;
    }

    //
    // List queries:
    //
    T * last() const noexcept
    {
        if (empty())
        {
            return nullptr;
        }
        return head->prev;
    }
    T * first() const noexcept
    {
        return head;
    }
    bool empty() const noexcept
    {
        return count == 0;
    }
    int size() const noexcept
    {
        return count;
    }

private:

    T * head  = nullptr; // Head of list, null if list empty. Circularly referenced.
    int count = 0;       // Number of items in the list.
};

#endif // LINKED_LIST_HPP
