#pragma once
#include <memory>

template <class T>
class Node {
    std::shared_ptr<Node> next;
    std::shared_ptr<Node> prev;
    T data;

public:
    Node(T data) : data(data), next(nullptr), prev(nullptr) {}

    std::shared_ptr<Node> getNext() const {
        return next;
    }

    void setNext(std::shared_ptr<Node> nextNode) {
        next = nextNode;
    }

    std::shared_ptr<Node> getPrev() const {
        return prev;
    }

    void setPrev(std::shared_ptr<Node> prevNode) {
        prev = prevNode;
    }

    T getData() const {
        return data;
    }

    void setData(T newData) {
        data = newData;
    }
};