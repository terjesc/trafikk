#pragma once

#include "controller.h"
#include "lockstepvalue.h"

#include <vector>
#include <map>
#include <queue>
#include <unordered_map>

const int AVERAGE_ROAD_LENGTH_PER_VEHICLE = 50000;
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

class Line; // Forward declaration

struct SpeedActionInfo
{
  SpeedAction speedAction; // What speed action to perform.
  unsigned int blockedBy; // ID of the Transport Network Packet to yield for
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
  Line * line;

  SpeedAction speedAction;
  unsigned int waitingFor;
  int waitedTime;
  bool physicallyBlocked;

  std::set<unsigned int> packetIDsToYieldFor;
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
    unsigned int id;
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

struct Coordinates
{
  float x, y, z;
};

class Line : public ControllerUser
{
  private:
    LockStepValue<std::vector<unsigned int> > _packets;
    std::vector<Line *> m_out;
    std::vector<Line *> m_in;
    std::vector<Line *> m_cooperating;
    std::vector<Line *> m_interfering;
    int m_length;
    std::map<Line*, std::vector<unsigned int> > m_packetInboxes;
    Coordinates m_beginPoint, m_endPoint;
    TransportNetwork * p_transportNetwork;

  public:
    static int totalNumberOfVehicles;

    Line(Controller *controller, Coordinates* beginPoint = NULL, Coordinates* endPoint = NULL, TransportNetwork * transportNetwork = NULL);
    Line(Controller *controller, Coordinates beginPoint, Coordinates endPoint, TransportNetwork * transportNetwork = NULL);

    void addPacket(unsigned int packetId);
    bool deliverPacket(Line * senderLine, unsigned int packetId);

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
    void draw();
    void moveRight(float distance = 0.2f);

    Coordinates coordinatesFromLineDistance(int distance);
};

