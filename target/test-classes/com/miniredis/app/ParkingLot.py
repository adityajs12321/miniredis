from abc import abstractmethod
from datetime import datetime
import random
import uuid

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