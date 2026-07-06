from dataclasses import dataclass
from typing import Optional, TypeVar, Generic

T = TypeVar("T")

KT = TypeVar("KT")
VT = TypeVar("VT")


class Node(Generic[T]):
    prev: "Node[T]"
    next: "Node[T]"
    data: T

    def __init__(self, data):
        self.prev = None
        self.next = None
        self.data = data


class DoublyLinkedList(Generic[T]):
    head: Node[T]
    tail: Node[T]

    def __init__(self):
        self.head = None
        self.tail = None

    def append(self, node: Node[T]):
        if not self.tail:
            self.tail = node
            self.head = node
            node.next = node
            node.prev = node
            return
        self.tail.next = node
        node.prev = self.tail
        node.next = self.head
        self.head.prev = node
        self.tail = node

    def prepend(self, node: Node[T]):
        if not self.head:
            self.tail = node
            self.head = node
            node.next = node
            node.prev = node
            return
        self.head.prev = node
        node.next = self.head
        node.prev = self.tail
        self.tail.next = node
        self.head = node

    def move_to_front(self, node: Node[T]):
        if node is self.head:
            return
        prev = node.prev
        next = node.next
        prev.next = node.next
        next.prev = node.prev
        if node is self.tail:
            self.tail = prev
        self.prepend(node)


@dataclass
class CacheEntry(Generic[KT, VT]):
    key: KT
    value: VT


class LruCache(Generic[KT, VT]):
    capacity: int
    size: int
    map: dict[KT, Node[CacheEntry[KT, VT]]]
    recencyList: DoublyLinkedList[CacheEntry[KT, VT]]

    def __init__(self, capacity: int):
        self.hashmap = dict()
        self.recencyList = DoublyLinkedList()
        self.capacity = capacity
        self.size = 0

    def get(self, key: KT) -> Optional[VT]:
        node = self.hashmap.get(key)
        if not node:
            return None
        self.recencyList.move_to_front(node)
        return node.data.value

    def put(self, key: KT, value: VT):
        node = self.hashmap.get(key)
        if not node:
            if (self.size == self.capacity):
                oldTail = self.recencyList.tail
                del self.hashmap[oldTail.data.key]
                self.recencyList.tail = oldTail.prev
                self.recencyList.tail.next = self.recencyList.head
                self.recencyList.head.prev = self.recencyList.tail
                self.size -= 1
            node = Node(CacheEntry(key, value))
            self.recencyList.prepend(node)
            self.hashmap[key] = node
            self.size += 1
            return
        node.data.value = value
        self.recencyList.move_to_front(node)
        return


cache: LruCache[int, int] = LruCache(3)
cache.put(1, 100)
cache.put(2, 200)
cache.put(3, 300)
cache.put(4, 400)

print(cache.get(1))
print(cache.get(2))
print(cache.get(3))
print(cache.get(4))
print(cache.get(1))
