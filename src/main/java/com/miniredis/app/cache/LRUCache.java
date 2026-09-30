package com.miniredis.app.cache;

import java.util.HashMap;

/**
 * LRUCache
 * 
 * A thread safe, fixed capacity LRU Cache.
 * @param <K>
 * @param <V>
 */

public class LRUCache<K, V> {
    private static class Node<K, V> {
        K key;
        V value;
        Node<K, V> next;
        Node<K, V> prev;

        Node(K key, V value) {
            this.key = key;
            this.value = value; 
            this.prev = null;
            this.next = null;
        }
    }

    private static class DoublyLinkedList<K, V> {
        Node<K, V> head;
        Node<K, V> tail;

        DoublyLinkedList() {
            this.head = new Node<>(null, null);
            this.tail = new Node<>(null, null);
            head.next = tail;
            tail.prev = head;
        }

        private void append(Node<K, V> node) {
            node.prev = tail.prev;
            node.next = tail;
            tail.prev.next = node;
            tail.prev = node;
        }

        private Node<K, V> popFirst() {
            Node<K, V> node = head.next;
            node.prev.next = node.next;
            node.next.prev = node.prev;
            return node;
        }

        private void removeNode(Node<K, V> node) {
            node.prev.next = node.next;
            node.next.prev = node.prev;
        }

        private void deleteAndAppend(Node<K, V> node) {
            removeNode(node);
            append(node);
        }
    }

    private final HashMap<K, Node<K, V>> nodes;
    private final DoublyLinkedList<K, V> list;
    private final int capacity;

    public LRUCache(int capacity) {
        this.nodes = new HashMap<>();
        this.list = new DoublyLinkedList<>();
        this.capacity = capacity;
    }

    public V get(K key) {
        if (nodes.containsKey(key)) {
            Node<K, V> node = nodes.get(key);
            list.deleteAndAppend(node);
            return node.value;
        }
        else {
            return null;
        }
    }

    public void put(K key, V value) {
        if (nodes.containsKey(key)) {
            Node<K, V> node = nodes.get(key);
            node.value = value;
            list.deleteAndAppend(node);
            return;
        }

        Node<K, V> node = new Node<>(key, value);

        if (nodes.size() >= capacity) {
            Node<K, V> popped = list.popFirst();
            nodes.remove(popped.key);
        }
        
        list.append(node);
        nodes.put(key, node);
    }
}