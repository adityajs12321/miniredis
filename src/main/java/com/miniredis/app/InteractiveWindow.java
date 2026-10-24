package com.miniredis.app;

import java.util.Scanner;

import com.miniredis.app.cache.LRUCache;

public class InteractiveWindow {
    public static void main(String[] args) {
        LRUCache<String, String> cache = new LRUCache<>(4);
        Scanner scanner = new Scanner(System.in);

        System.out.println("MiniRedis REPL. Commands: get <key>, put <key> <value>, exit");

        while (true) {
            System.out.print("> ");
            if (!scanner.hasNextLine()) {
                break;
            }

            String[] input = scanner.nextLine().trim().split("\\s+");
            String command = input[0].toLowerCase();

            if (command.equals("get") && input.length == 2) {
                String value = cache.get(input[1]);
                System.out.println(value == null ? "(nil)" : value);
            }
            else if (command.equals("put") && input.length == 3) {
                cache.put(input[1], input[2]);
                System.out.println("OK");
            }
            else if (command.equals("exit")) {
                break;
            }
            else {
                System.out.println("Unknown command. Usage: get <key> | put <key> <value> | exit");
            }
        }

        scanner.close();
    }
}
