package com.miniredis.app;

import com.miniredis.app.cache.LRUCache;

public class Main {
    public static void main(String[] args) {
        System.out.println("Initiating LRU Cache");

        LRUCache<Integer, Integer> cache = new LRUCache<>(3);

        int[][] arr = {{1, 1}, {2, 2}, {1}, {3, 3}, {2}, {4, 4}, {1}, {3}, {4}};
        int[] op = {2, 2, 1, 2, 1, 2, 1, 1, 1};
        for (int i = 0; i < arr.length; i++) {
            if (op[i] == 1) {
                if (cache.get(arr[i][0]) == null ){
                    System.out.println(-1);
                }
                else {
                    System.out.println(cache.get(arr[i][0]));

                }
            }
            else {
                cache.put(arr[i][0], arr[i][1]);
                System.out.println("null");
            }
        }
    }
}