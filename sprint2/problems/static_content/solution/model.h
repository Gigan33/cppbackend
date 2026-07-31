#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace model {

using Dimension = int;

struct Point {
    int x, y;
};

struct Rectangle {
    Rectangle() = default;
    Rectangle(Point p1, Point p2) : p1(p1), p2(p2) {}
    
    Point p1, p2;
};

struct Offset {
    int dx, dy;
};

class Road {
public:
    Road(Point start, Point end) : start_(start), end_(end) {}
    
    const Point& GetStart() const { return start_; }
    const Point& GetEnd() const { return end_; }
    
private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) : bounds_(bounds) {}
    
    const Rectangle& GetBounds() const { return bounds_; }
    
private:
    Rectangle bounds_;
};

class Office {
public:
    Office(std::string id, Point position, Offset offset)
        : id_(std::move(id)), position_(position), offset_(offset) {}
    
    const std::string& GetId() const { return id_; }
    const Point& GetPosition() const { return position_; }
    const Offset& GetOffset() const { return offset_; }
    
private:
    std::string id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    Map(std::string id, std::string name)
        : id_(std::move(id)), name_(std::move(name)) {}
    
    const std::string& GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    
    void AddRoad(Road road) { roads_.push_back(std::move(road)); }
    void AddBuilding(Building building) { buildings_.push_back(std::move(building)); }
    void AddOffice(Office office) { offices_.push_back(std::move(office)); }
    
    const std::vector<Road>& GetRoads() const { return roads_; }
    const std::vector<Building>& GetBuildings() const { return buildings_; }
    const std::vector<Office>& GetOffices() const { return offices_; }
    
private:
    std::string id_;
    std::string name_;
    std::vector<Road> roads_;
    std::vector<Building> buildings_;
    std::vector<Office> offices_;
};

class Game {
public:
    void AddMap(Map map) {
        maps_.push_back(std::move(map));
    }
    
    const std::vector<Map>& GetMaps() const { return maps_; }
    
    const Map* FindMap(const std::string& id) const {
        for (const auto& map : maps_) {
            if (map.GetId() == id) {
                return &map;
            }
        }
        return nullptr;
    }
    
private:
    std::vector<Map> maps_;
};

}  // namespace model
