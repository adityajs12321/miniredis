package com.miniredis.app;

abstract class Logger {
    int level;

    public Logger(int level) {
        this.level = level;
    }
}

class PaymentGateway {
    void processPayment(double amount) {
        System.out.println("Processing payment of $" + amount);
    }
}

class Order {
    private PaymentGateway gateway;

    public Order(PaymentGateway gateway) {
        this.gateway = gateway;
    }

    public void checkout() {
        gateway.processPayment(100.0);
    }
}

class FileLogger extends Logger {
    String filePath;

    FileLogger(int level, String filePath) {
        super(level);
        this.filePath = filePath;
    }
}

public class BackgroundReaperThread {

}
