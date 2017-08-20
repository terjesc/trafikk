#pragma once

#include "controller.h"
#include "lockstepvalue.h"

#include <vector>
#include <map>

const int AVERAGE_NUMBER_OF_VEHICLES = 2;
const int SPEED = 16667; // 16667 mm/s = 60 km/h
const int VEHICLE_LENGTH = 4000;
const int VEHICLE_HEIGHT = 1750;
const int VEHICLE_WIDTH = 1750;

const int BRAKE_ACCELERATION = 3500;
const int SPEEDUP_ACCELERATION = 1500;

const int ZOOM_FACTOR = 10000.0f;

enum SpeedAction
{
  BRAKE,
  MAINTAIN,
  INCREASE
};

struct Blocker
{
  int speed;
  int distance;
};

struct VehicleInfo
{
  int speed;
  int preferredSpeed;
  int position;
  float color[3];
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
    std::vector<Line *> m_cooperating;
    std::vector<Line *> m_interfering;
    int m_length;
    std::map<Line*, std::vector<VehicleInfo> > m_vehicleInboxes;
    Coordinates m_beginPoint, m_endPoint;

  public:
    static int totalNumberOfVehicles;

    Line(Controller *controller, Coordinates* beginPoint = NULL, Coordinates* endPoint = NULL);
    Line(Controller *controller, Coordinates beginPoint, Coordinates endPoint);

    void addVehicle(VehicleInfo vehicleInfo);
    void deliverVehicle(Line * senderLine, VehicleInfo vehicleInfo);

    SpeedAction forwardGetSpeedAction(Blocker requestingVehicle, Line* requestingLine, int requestingVehicleIndex = -1);
    SpeedAction backwardMergeGetSpeedAction(Blocker requestingVehicle, Line* requestingLine);
    SpeedAction backwardYieldGetSpeedAction(Blocker requestingVehicle, Line* requestingLine);

    void addIn(Line * in);
    void addOut(Line * out);
    void addCooperating(Line * cooperating);
    void addInterfering(Line * interfering);
    int getLength();
    virtual void tick(int tickType);
    void tick0();
    void tick1();
    void print();
    void draw();
    std::vector<VehicleInfo> getVehicles();
    void moveRight(float distance = 0.2f);
};



