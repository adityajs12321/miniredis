package com.miniredis.app;

import java.util.Scanner;

import com.miniredis.app.cache.LRUCache;

public class InteractiveWindow {
    public static void main(String[] args) {
        LRUCache<String, String> cache = new LRUCache<>(4);

        while (true) {
            Scanner scanner = new Scanner(System.in);
            String str = scanner.toString();
            String[] input = str.split(" ");

            if (input[0] == "get") {
                if cache
                System.out.println(cache.get(input[1]));
            }
            else if (input[1] == "put") {
                cache.put(input[1], input[2]);
                System.out.println("Inserted.");
            }
            else {
                System.out.println("sonion.");
            }

            scanner.close();
        }
    }
}
