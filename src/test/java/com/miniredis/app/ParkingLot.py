from abc import abstractmethod
from datetime import datetime
import random
import uuid
from collections import deque

class Vehicle:
    size = None
    def __init__(self):
        self.entryTime = datetime.now()

class Motorcycle(Vehicle):
    size = "small"

class Car(Vehicle):
    size = "medium"

class Truck(Vehicle):
    size = "large"

parkingSpots = []
    
for i in range(2):
    level = []
    for j in range(10):
        level.append({chr(ord('A') + i): {"Available": random.randint(0, 1) == 1, "Size": random.randint(1, 3)}})
    parkingSpots.extend(level)

print(parkingSpots)

vehicles = [Car(), Car(), Motorcycle(), Truck()]

queue = deque(vehicles)

for i in range(len(parkingSpots)):
    if queue:
        vehicle = queue.popleft()
        spot = parkingSpots[i]
        for key in spot:
            if spot[key]["Available"] and spot[key]["Size"] >= (1 if vehicle.size == "small" else 2 if vehicle.size == "medium" else 3):
                spot[key]["Available"] = False
                print(f"Parked {vehicle.__class__.__name__} at spot {key} on level {i // 10 + 1}")
                break