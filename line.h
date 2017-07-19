#pragma once

#include "controller.h"
#include "lockstepvalue.h"

#include <vector>
#include <map>

const int AVERAGE_NUMBER_OF_VEHICLES = 5;
const int SPEED = 50;
const int VEHICLE_LENGTH = 500;

struct Blocker
{
  int speed;
  int distance;
};

struct VehicleInfo
{
  int speed;
  int preferedSpeed;
  int position;
};

struct Coordinates
{
  float x, y, z;
};

class Line : public ControllerUser
{
  private:
    LockStepValue<Blocker> _blocker;
    LockStepValue<std::vector<VehicleInfo> > _vehicles;
    LockStepValue<int> tickNumber;
    std::vector<Line *> m_out;
    std::vector<Line *> m_in;
    int m_length;
    std::map<Line*, std::vector<VehicleInfo> > m_vehicleInbox;
    Coordinates m_beginPoint, m_endPoint;

  public:
    static int totalNumberOfVehicles;

    Line(Controller *controller, Coordinates* beginPoint = NULL, Coordinates* endPoint = NULL);
    void addVehicle(VehicleInfo vehicleInfo);
    void deliverVehicle(Line * senderLine, VehicleInfo vehicleInfo);
    Blocker getBlocker(int maxDistance);
    void addIn(Line * in);
    void addOut(Line * out);
    int getLength();
    virtual void tick(int tickType);
    void tick0();
    void tick1();
    void print();
    void draw();
    std::vector<VehicleInfo> getVehicles();
};


