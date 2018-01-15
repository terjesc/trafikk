#pragma once

#include "controller.h"
#include "lockstepvalue.h"

#include <vector>
#include <map>
#include <queue>
#include <unordered_map>

const int AVERAGE_ROAD_LENGTH_PER_VEHICLE = 100000;
const int MMPS_PER_KMPH = 278; // km/h to mm/s conversion factor
const int SPEED = 30 * MMPS_PER_KMPH;
const int VEHICLE_LENGTH = 4000;
const int VEHICLE_HEIGHT = 1750;
const int VEHICLE_WIDTH = 1750;

const int BRAKE_ACCELERATION = 3500;
const int SPEEDUP_ACCELERATION = 1500;

const int ZOOM_FACTOR = 5000.0f;

enum SpeedAction
{
  BRAKE,
  MAINTAIN,
  INCREASE
};

// TODO remove, use VehicleInfo or TransportNetworkPacket directly instead
struct Blocker
{
  int speed;
  int distance;
  int index;
};

class Line; // Forward declaration

// TODO remove SpeedActionCulprit entirely?
struct SpeedActionCulprit
{
  Line * line; // The line on which the culprit was seen.
  int distance; // The spatial position of the culprit  on the line.
  int index; // The ordered position of the culprit on the line.
};

struct SpeedActionInfo
{
  SpeedAction speedAction; // What speed action to perform.
  SpeedActionCulprit culprit; // TODO remove?
  int timeSinceLastActionChange; // TODO remove; should be stored in packet instead
  unsigned int blockedBy; // Index of the Transport Network Packet to yield for
  bool physicallyBlocked; // Whether or not blockedBy physically blocks the path
};

struct Vehicle
{
  float color[3];
};

struct TransportNetworkPacketMutableData
{
  int speed;
  int positionAtLine;

  Line * line;     // For addressing; do we even need this?
//  int indexInLine; // -"-

  SpeedAction speedAction;
  int waitingFor;
  int waitedTime;
  bool physicallyBlocked;

  std::set<unsigned int> packetIdsToYieldFor;
  std::deque<Line *> route;

  void addRoutePoint(Line * line)
  {
    route.push_back(line);
  }

  void removeRoutePoint()
  {
    route.pop_front();
  }

  void fillRoute(Line * line);
  Line * getNextRoutePoint(Line *line) const;
};

class TransportNetworkPacket
{
  public:
    int id;
    Vehicle * vehicle;
    int length;
    int preferredSpeed;

    LockStepValue<TransportNetworkPacketMutableData> mutableData;

    TransportNetworkPacket(Controller * controller);
};

class TransportNetwork
{
  private:
    Controller * p_controller;
    std::vector<Line *> m_lines;
    std::unordered_map<unsigned int, TransportNetworkPacket> m_packets;
    unsigned int nextPacketIndex();
  public:
    TransportNetwork(Controller * controller);

    unsigned int addPacket(TransportNetworkPacket packet, Line * line);
    TransportNetworkPacket * getPacket(unsigned int packetIndex);
    bool loadLinesFromFile(std::string fileName);

    void draw();
};

class VehicleInfo
{
  public:
    int id;
    int speed;
    int preferredSpeed;
    int position;
    float color[3];
    std::deque<Line *> route;
    SpeedActionInfo speedActionInfo;
    std::set<int> vehiclesToYieldFor;

    void addRoutePoint(Line * line)
    {
      route.push_back(line);
    }

    void removeRoutePoint()
    {
      route.pop_front();
    }

    void fillRoute(Line * line);
    Line * getNextRoutePoint(Line *line);
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
    LockStepValue<std::vector<unsigned int> > _packets;
    LockStepValue<int> tickNumber;
    std::vector<Line *> m_out;
    std::vector<Line *> m_in;
    std::vector<Line *> m_cooperating;
    std::vector<Line *> m_interfering;
    int m_length;
    std::map<Line*, std::vector<VehicleInfo> > m_vehicleInboxes;
    std::map<Line*, std::vector<unsigned int> > m_packetInboxes;
    Coordinates m_beginPoint, m_endPoint;
    TransportNetwork * p_transportNetwork;

  public:
#if 0
    void makeRoutesForVehicles();
#endif
    static int totalNumberOfVehicles;

    Line(Controller *controller, Coordinates* beginPoint = NULL, Coordinates* endPoint = NULL, TransportNetwork * transportNetwork = NULL);
    Line(Controller *controller, Coordinates beginPoint, Coordinates endPoint, TransportNetwork * transportNetwork = NULL);

    void addVehicle(VehicleInfo vehicleInfo);
    void deliverVehicle(Line * senderLine, VehicleInfo vehicleInfo);

    void addPacket(unsigned int packetId);
    bool deliverPacket(Line * senderLine, unsigned int packetId);

    SpeedActionInfo forwardGetSpeedAction(VehicleInfo *requestingVehicle, Line* requestingLine, int requestingVehicleIndex = -1);
    SpeedActionInfo backwardMergeGetSpeedAction(VehicleInfo *requestingVehicle, Line* requestingLine);
    SpeedActionInfo backwardYieldGetSpeedAction(VehicleInfo *requestingVehicle, Line* requestingLine);

    SpeedActionInfo forwardGetSpeedAction(TransportNetworkPacket  * requestingPacket,
                                          int                       requestingPacketIndex,
                                          Line                    * requestingLine,
                                          int                       requestingPacketPosition);
    SpeedActionInfo backwardMergeGetSpeedAction(TransportNetworkPacket  * requestingPacket,
                                                Line                    * requestingLine,
                                                int                       requestingPacketPosition);
    SpeedActionInfo backwardYieldGetSpeedAction(TransportNetworkPacket  * requestingPacket,
                                                Line                    * requestingLine,
                                                int                       requestingPacketPosition);

    void addIn(Line * in);
    void addOut(Line * out);
    void addCooperating(Line * cooperating);
    void addInterfering(Line * interfering);

    std::vector<Line *> getOut();

    int getLength();
    virtual void tick(int tickType);
    void tick0();
    void tick1();
    void print();
    void draw();
    std::vector<VehicleInfo> getVehicles();
    VehicleInfo const * getVehicle(int index) const;
    void moveRight(float distance = 0.2f);

    Coordinates coordinatesFromVehicleIndex(int index);
    Coordinates coordinatesFromLineDistance(int distance);
};

