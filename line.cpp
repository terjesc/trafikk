
#include "line.h"

#include <climits>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <GL/glew.h>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <unordered_map>

static const Vehicle DEFAULT_VEHICLE = {{0.5, 0.5, 0.5}};

void TransportNetworkPacketMutableData::fillRoute(Line * line)
{
  if (route.size())
  {
    for (std::deque<Line *>::const_iterator it = route.cbegin();
        it != route.cend() && *it != line; ++it)
    {
      removeRoutePoint();
    }
  }

  if (route.empty())
  {
    addRoutePoint(line);
  }

  while (route.size() < 10)
  {
    if (route.back()->getOut().size() == 0)
    {
      break;
    }
    Line * routeExtension = route.back()->getOut()[rand() % route.back()->getOut().size()];
    addRoutePoint(routeExtension);
  }
}

Line * TransportNetworkPacketMutableData::getNextRoutePoint(Line * line) const
{
  if (line == NULL)
  {
    return NULL;
  }

  if (route.empty())
  {
    return NULL;
  }

  for (std::deque<Line *>::const_iterator it = route.cbegin();
      it != route.cend(); ++it)
  {
    if (it != route.cend() && *it == line)
    {
      it++;
      if (it != route.cend())
      {
        return *it;
      }
      else
      {
        return NULL;
      }
    }
  }

  return NULL;
}

TransportNetworkPacket::TransportNetworkPacket(Controller * controller)
  : mutableData(controller)
{
}

unsigned int TransportNetwork::nextPacketIndex()
{
  static unsigned int nextPacketIndex = 0;
  while (m_packets.find(nextPacketIndex) != m_packets.end())
  {
    nextPacketIndex++;
  }
  return nextPacketIndex;
}

TransportNetwork::TransportNetwork(Controller *controller)
{
  p_controller = controller;
}

unsigned int TransportNetwork::addPacket(TransportNetworkPacket packet, Line * line)
{
  unsigned int packetIndex = nextPacketIndex();
  m_packets.insert({packetIndex, packet});
  line->deliverPacket(line, packetIndex);
  return packetIndex;
}

TransportNetworkPacket * TransportNetwork::getPacket(unsigned int packetIndex)
{
  std::unordered_map<unsigned int, TransportNetworkPacket>::iterator packetIt
      = m_packets.find(packetIndex);
  if (packetIt == m_packets.end())
  {
    return NULL;
  }
  else
  {
    return &packetIt->second;
  }
}

bool TransportNetwork::loadLinesFromFile(std::string fileName)
{
  std::map<int, Line*> lineMap;
  std::vector<std::pair<int, int> > inLines;
  std::vector<std::pair<int, int> > outLines;
  std::vector<std::pair<int, int> > mergeLines;
  std::vector<std::pair<int, int> > yieldLines;

  std::string line;
  std::ifstream lineFile(fileName.c_str());
  // TODO Error handling: Return false if opening file fails.

  while (std::getline(lineFile, line))
  {
    if (line.length() == 0 || line[0] == '#')
    {
      continue;
    }

    std::stringstream lineStream;
    lineStream << line;

    int lineNumber;
    lineStream >> lineNumber;

    Coordinates begin, end;

    lineStream >> begin.x;
    lineStream >> begin.y;
    lineStream >> begin.z;
    lineStream >> end.x;
    lineStream >> end.y;
    lineStream >> end.z;

    Line *newLine = new Line(p_controller, begin, end, this);
    lineMap.insert(std::pair<int,Line*>(lineNumber, newLine));

    int inCount;
    lineStream >> inCount;
    for (int i = 0; i < inCount; ++i)
    {
      int inLine;
      lineStream >> inLine;
      inLines.push_back(std::pair<int, int>(lineNumber, inLine));
    }

    int outCount;
    lineStream >> outCount;
    for (int i = 0; i < outCount; ++i)
    {
      int outLine;
      lineStream >> outLine;
      outLines.push_back(std::pair<int, int>(lineNumber, outLine));
    }

    int mergeCount;
    lineStream >> mergeCount;
    for (int i = 0; i < mergeCount; ++i)
    {
      int mergeLine;
      lineStream >> mergeLine;
      mergeLines.push_back(std::pair<int, int>(lineNumber, mergeLine));
    }

    int yieldCount;
    lineStream >> yieldCount;
    for (int i = 0; i < yieldCount; ++i)
    {
      int yieldLine;
      lineStream >> yieldLine;
      yieldLines.push_back(std::pair<int, int>(lineNumber, yieldLine));
    }
  }
  lineFile.close();

  for (std::vector<std::pair<int, int> >::const_iterator it = inLines.cbegin();
      it != inLines.cend(); ++it)
  {
    if (lineMap.count(it->first) && lineMap.count(it->second))
    {
      (lineMap[it->first])->addIn(lineMap[it->second]);
    }
  }

  for (std::vector<std::pair<int, int> >::const_iterator it = outLines.cbegin();
      it != outLines.cend(); ++it)
  {
    if (lineMap.count(it->first) && lineMap.count(it->second))
    {
      (lineMap[it->first])->addOut(lineMap[it->second]);
    }
  }

  for (std::vector<std::pair<int, int> >::const_iterator it = mergeLines.cbegin();
      it != mergeLines.cend(); ++it)
  {
    if (lineMap.count(it->first) && lineMap.count(it->second))
    {
      (lineMap[it->first])->addCooperating(lineMap[it->second]);
    }
  }

  for (std::vector<std::pair<int, int> >::const_iterator it = yieldLines.cbegin();
      it != yieldLines.cend(); ++it)
  {
    if (lineMap.count(it->first) && lineMap.count(it->second))
    {
      (lineMap[it->first])->addInterfering(lineMap[it->second]);
    }
  }

  for (std::map<int, Line*>::const_iterator lineIt = lineMap.cbegin();
      lineIt != lineMap.cend(); ++lineIt)
  {
    m_lines.push_back(lineIt->second);
  }

  return true;
}

void TransportNetwork::draw()
{
  for (std::vector<Line *>::iterator lineIt = m_lines.begin();
      lineIt != m_lines.end(); ++lineIt)
  {
    (*lineIt)->draw();
  }
}

void VehicleInfo::fillRoute(Line * line)
{
  if (route.size())
  {
    for (std::deque<Line *>::const_iterator it = route.cbegin();
        it != route.cend() && *it != line; ++it)
    {
      removeRoutePoint();
    }
  }

  if (route.empty())
  {
    addRoutePoint(line);
  }

  while (route.size() < 10)
  {
    Line * routeExtension = route.back()->getOut()[rand() % route.back()->getOut().size()];
    addRoutePoint(routeExtension);
  }
}

Line * VehicleInfo::getNextRoutePoint(Line * line)
{
  if (line == NULL)
  {
    return NULL;
  }

  if (route.empty())
  {
    return NULL;
  }

  for (std::deque<Line *>::const_iterator it = route.cbegin();
      it != route.cend(); ++it)
  {
    if (it != route.cend() && *it == line)
    {
      it++;
      if (it != route.cend())
      {
        return *it;
      }
      else
      {
        return NULL;
      }
    }
  }

  return NULL;
}

Line::Line(Controller *controller, Coordinates* beginPoint, Coordinates* endPoint, TransportNetwork * transportNetwork)
  : ControllerUser(controller),
    _blocker(controller, {0, 0}),
    _vehicles(controller),
    _packets(controller),
    tickNumber(controller, 0)
{
  p_transportNetwork = transportNetwork;

  // Assign or randomize the physical starting point of the line
  if (beginPoint != NULL)
  {
    m_beginPoint = *beginPoint;
  }
  else
  {
    m_beginPoint = { static_cast<float>(rand() % 20) - 10.0f,
                     static_cast<float>(rand() % 20) - 10.0f,
                     0.0f };
  }

  // Assign or randomize the physical ending point of the line
  if (endPoint != NULL)
  {
    m_endPoint = *endPoint;
  }
  else
  {
    m_endPoint = { static_cast<float>(rand() % 20) - 10.0f,
                   static_cast<float>(rand() % 20) - 10.0f,
                   0.0f };
  }

  m_length = ZOOM_FACTOR * sqrt(pow(m_beginPoint.x - m_endPoint.x, 2) + pow(m_beginPoint.y - m_endPoint.y, 2));

  // Add a random number of vehicles
  int numberOfVehicles = m_length / AVERAGE_ROAD_LENGTH_PER_VEHICLE;
  int roadFractionLeft = m_length % AVERAGE_ROAD_LENGTH_PER_VEHICLE;
  if (rand() % AVERAGE_ROAD_LENGTH_PER_VEHICLE < roadFractionLeft)
  {
    numberOfVehicles += 1;
  }
  numberOfVehicles = rand() % (1 + (2 * numberOfVehicles));

  // After the change to TransportNetworkPackets, use that instead...
  if (p_transportNetwork != NULL)
  {
    // Add TransportNetworkPackets instead of vehicles
    for (int i = 0; i < numberOfVehicles; ++i)
    {
      TransportNetworkPacket packet(controller);

      Vehicle * vehicle = new Vehicle;
      vehicle->color[0] = 0.4f + ((rand() % 50) / 100.0f);
      vehicle->color[1] = 0.4f + ((rand() % 50) / 100.0f);
      vehicle->color[2] = 0.4f + ((rand() % 50) / 100.0f);
      packet.vehicle = vehicle;

      packet.length = VEHICLE_LENGTH;
      packet.preferredSpeed = SPEED - 1000 + (rand() % 2001);

      TransportNetworkPacketMutableData mutableData;
      mutableData.speed = SPEED;
      mutableData.positionAtLine = m_length - (i * m_length / numberOfVehicles) - 1;
      mutableData.speedAction = INCREASE;
      mutableData.waitingFor = -1;
      mutableData.waitedTime = 0;
      mutableData.physicallyBlocked = false;
      packet.mutableData.initialize(mutableData);

      p_transportNetwork->addPacket(packet, this);
    }
  }
  else // Legacy code: Adds VehicleInfo. TODO remove
  {
    std::vector<VehicleInfo> v;
    for (int i = 0; i < numberOfVehicles; ++i)
    {
      int id = totalNumberOfVehicles + i + 1;
      int preferedSpeed = SPEED - 1000 + (rand() % 2001);
      int position = m_length - (i * m_length / numberOfVehicles);
      SpeedActionInfo speedActionInfo = {INCREASE, {NULL, 0, -1}, 0};
      VehicleInfo vi = {id, SPEED, preferedSpeed, position, {0}, std::deque<Line*>(), speedActionInfo};
      vi.color[0] = 0.4f + ((rand() % 50) / 100.0f);
      vi.color[1] = 0.4f + ((rand() % 50) / 100.0f);
      vi.color[2] = 0.4f + ((rand() % 50) / 100.0f);
      v.push_back(vi);
    }
    _vehicles.initialize(v);
  }
  totalNumberOfVehicles += numberOfVehicles;
}

Line::Line(Controller *controller, Coordinates beginPoint, Coordinates endPoint, TransportNetwork * transportNetwork)
  : Line(controller, &beginPoint, &endPoint, transportNetwork)
{
}

int Line::totalNumberOfVehicles = 0;

void Line::addVehicle(VehicleInfo vehicleInfo)
{
  _vehicles.THEN().push_back(vehicleInfo);
}

void Line::addPacket(unsigned int packetId)
{
  _packets.THEN().push_back(packetId);
}

void Line::deliverVehicle(Line * senderLine, VehicleInfo vehicleInfo)
{
  vehicleInfo.fillRoute(this);

  if (vehicleInfo.position < m_length)
  {
    m_vehicleInboxes[senderLine].push_back(vehicleInfo);
  }
  else
  {
    vehicleInfo.position -= m_length;
    if (!m_out.empty())
    {
      vehicleInfo.route.front()->deliverVehicle(senderLine, vehicleInfo);
    }
  }
}

bool Line::deliverPacket(Line * senderLine, unsigned int packetId)
{
  if (p_transportNetwork == NULL)
  {
    return false;
  }

  TransportNetworkPacket * packet = p_transportNetwork->getPacket(packetId);
  
  if (packet == NULL)
  {
    return false;
  }

  packet->mutableData.THEN().fillRoute(this);
  int positionAtLine = packet->mutableData.THEN().positionAtLine;

  if (positionAtLine >= 0 && positionAtLine < m_length)
  {
    m_packetInboxes[senderLine].push_back(packetId);
    return true;
  }
  else
  {
    packet->mutableData.THEN().positionAtLine -= m_length;
    if (!m_out.empty())
      // FIXME should probably check that route.front() exists
    {
      packet->mutableData.THEN().route.front()->deliverPacket(senderLine, packetId);
    }

  }

  return false;
}

static int calculateBrakePoint(int currentPosition, int speed)
{
  int brakeLength = pow(speed, 2) / (2 * BRAKE_ACCELERATION);
  return currentPosition + brakeLength;
}

static int calculateBrakePoint(VehicleInfo const *vehicleInfo)
{
  return calculateBrakePoint(vehicleInfo->position, vehicleInfo->speed);
}

static int calculateBrakePoint(Blocker const *blocker)
{
  return calculateBrakePoint(blocker->distance, blocker->speed);
}

/*
 * Forward search for whether a vehicle may accelerate or must brake
 *
 * requestingVehicle.distance relative to this line
 */
SpeedActionInfo Line::forwardGetSpeedAction(TransportNetworkPacket *requestingPacket,
                                            int                     requestingPacketIndex,
                                            Line                   *requestingLine,
                                            int                     requestingPacketPosition)
{
  SpeedActionInfo result = {.speedAction = INCREASE, .culprit = {NULL, 0, -1}, .timeSinceLastActionChange = 0, .blockedBy = INT_MAX, .physicallyBlocked = false};

//  int requestingPacketPosition = requestingLinePosition + requestingPacket->mutableData.NOW().positionAtLine;
  int requestingPacketSpeed = requestingPacket->mutableData.NOW().speed;
  int brakePoint = calculateBrakePoint(requestingPacketPosition, requestingPacketSpeed);
  int searchPoint = brakePoint
                  + (2 * requestingPacketSpeed)
                  + (2 * VEHICLE_LENGTH);

  // Check for vehicles to yield for in this line
  int nextPacketBrakePoint = INT_MAX;
  int nextPacketIndex = -1;
  unsigned int nextPacketID = INT_MAX;

  bool checkForBlocking = false;

  if (requestingPacketIndex == -1 // The requesting packet is external to this line,
      && !_packets.NOW().empty()) // and there is a blocking packet in this line
  {
    TransportNetworkPacket * nextPacket = p_transportNetwork->getPacket(_packets.NOW().back());

    if (nextPacket->mutableData.NOW().positionAtLine < searchPoint) //within search distance
    {
      int nextPacketDistance = nextPacket->mutableData.NOW().positionAtLine;
      int nextPacketSpeed = nextPacket->mutableData.NOW().speed;

      nextPacketBrakePoint = calculateBrakePoint(nextPacketDistance, nextPacketSpeed);

      nextPacketIndex = _packets.NOW().size() - 1;
      nextPacketID = _packets.NOW()[nextPacketIndex];

      checkForBlocking = true;
    }
  }
  else if (requestingPacketIndex > 0 // The requesting vehicle is in this line (but not first)
      && requestingPacketIndex < static_cast<int>(_packets.NOW().size()))
  {
    nextPacketIndex = requestingPacketIndex - 1;
    nextPacketID = _packets.NOW()[nextPacketIndex];

    TransportNetworkPacket * nextPacket = p_transportNetwork->getPacket(nextPacketID);

    if (nextPacket == requestingPacket)
    {
      std::cerr << "Serious error: Transport Network Packet " << nextPacketID << " is waiting for itself!" << std::endl;
    }
    int nextPacketDistance = nextPacket->mutableData.NOW().positionAtLine;
    int nextPacketSpeed = nextPacket->mutableData.NOW().speed;

    nextPacketBrakePoint = calculateBrakePoint(nextPacketDistance, nextPacketSpeed);

    checkForBlocking = true;
  }

  if (checkForBlocking)
  {
    if ((brakePoint + requestingPacketSpeed + VEHICLE_LENGTH) >= nextPacketBrakePoint)
    {
      // Must brake in order not to risk colliding
      return {.speedAction = BRAKE, .culprit = {NULL, 0, -1}, .timeSinceLastActionChange = 0, .blockedBy = nextPacketID, .physicallyBlocked = true};
    }
    else if ((brakePoint + requestingPacketSpeed + SPEEDUP_ACCELERATION + VEHICLE_LENGTH)
        >= std::max(0, nextPacketBrakePoint - BRAKE_ACCELERATION))
    {
      // May not safely increase the speed
      return {.speedAction = MAINTAIN, .culprit = {NULL, 0, -1}, .timeSinceLastActionChange = 0, .blockedBy = nextPacketID, .physicallyBlocked = true};
    }
  }

  // Continue searches from end of line,
  // but only if search does not distance out before that.
  if (searchPoint > m_length)
  {
    // Perform backward search on interfering lines,
    // for yielding for that traffic.
#if 0
    for (std::vector<Line*>::const_iterator interferingLineIt = m_interfering.cbegin();
        interferingLineIt != m_interfering.cend(); ++interferingLineIt)
    {
      if (*interferingLineIt == requestingLine)
      {
        continue;
      }

      VehicleInfo vehicleAtEndOfLine = *requestingVehicle;
      vehicleAtEndOfLine.position -= m_length;
      SpeedActionInfo yieldingResult =
        (*interferingLineIt)->backwardYieldGetSpeedAction(&vehicleAtEndOfLine, requestingLine);

      if (yieldingResult.speedAction < result.speedAction)
      {
        result = yieldingResult;
      }
    }
#endif

#if 0
    // Perform backward search on cooperating lines,
    // for mutual merging with that traffic.
    for (std::vector<Line*>::const_iterator cooperatingLineIt = m_cooperating.cbegin();
        cooperatingLineIt != m_cooperating.cend(); ++cooperatingLineIt)
    {
      // Skip the line containing the vehicle itself.
      // Otherwise it would brake in order not to collide with itsef...
      if (*cooperatingLineIt == requestingLine)
      {
        continue;
      }

      VehicleInfo vehicleAtEndOfLine = *requestingVehicle;
      vehicleAtEndOfLine.position -= m_length;
      SpeedActionInfo mergingResult =
          (*cooperatingLineIt)->backwardMergeGetSpeedAction(&vehicleAtEndOfLine, requestingLine);

      if (mergingResult.speedAction < result.speedAction)
      {
        result = mergingResult;
      }
    }
#endif

    // Perform forward search along the route
    Line * outboundLine = requestingPacket->mutableData.NOW().getNextRoutePoint(this);
    if (outboundLine != NULL
        && outboundLine != requestingLine)
    {
//      VehicleInfo vehicleAtEndOfLine = *requestingVehicle;
//      vehicleAtEndOfLine.position -= m_length;

//    SpeedActionInfo forwardGetSpeedAction
//    (TransportNetworkPacket *requestingPacket,
//     int                     requestingPacketIndex,
//     Line                   *requestingLine,
//     int                     requestingLinePosition);

      SpeedActionInfo outboundResult =
          outboundLine->forwardGetSpeedAction(requestingPacket, -1, requestingLine, requestingPacketPosition - m_length);

      if (outboundResult.speedAction < result.speedAction)
      {
        result = outboundResult;
      }
    }
  }

  return result;
}

/*
 * Forward search for whether a vehicle may accelerate or must brake
 *
 * requestingVehicle.distance relative to this line
 */
SpeedActionInfo Line::forwardGetSpeedAction(VehicleInfo *requestingVehicle,
                                            Line        *requestingLine,
                                            int          requestingVehicleIndex)
{
  SpeedActionInfo result = {INCREASE, {NULL, 0, -1}, 0};

  int brakePoint = calculateBrakePoint(requestingVehicle);
  int searchPoint = brakePoint
                  + (2 * requestingVehicle->speed)
                  + (2 * VEHICLE_LENGTH);

  // Check for vehicles to yield for in this line
  int nextVehicleBrakePoint = INT_MAX;
  int nextVehicleDistance = 0;
  int nextVehicleIndex = -1;

  if (requestingVehicleIndex == -1 // The requesting vehicle is external to this line,
      && !_vehicles.NOW().empty() // and there is a blocker in this line
      && _blocker.NOW().distance < searchPoint) // within search distance
  {
    nextVehicleDistance = _blocker.NOW().distance;
    nextVehicleBrakePoint = calculateBrakePoint(&_blocker.NOW());
    nextVehicleIndex = _blocker.NOW().index;
  }
  else if (requestingVehicleIndex > 0 // The requesting vehicle is in this line (but not first)
      && requestingVehicleIndex < static_cast<int>(_vehicles.NOW().size()))
  {
    nextVehicleIndex = requestingVehicleIndex - 1;
    nextVehicleDistance = _vehicles.NOW()[nextVehicleIndex].position;
    nextVehicleBrakePoint = calculateBrakePoint(&_vehicles.NOW()[nextVehicleIndex]);
  }

  if ((brakePoint + requestingVehicle->speed + VEHICLE_LENGTH) >= nextVehicleBrakePoint)
  {
    // Must brake in order not to risk colliding
    return {BRAKE, {this, nextVehicleDistance - (VEHICLE_LENGTH / 2), nextVehicleIndex}, 0};
  }
  else if ((brakePoint + requestingVehicle->speed + SPEEDUP_ACCELERATION + VEHICLE_LENGTH)
      >= std::max(0, nextVehicleBrakePoint - BRAKE_ACCELERATION))
  {
    // May not safely increase the speed
    return {MAINTAIN, {this, nextVehicleDistance - (VEHICLE_LENGTH / 2), nextVehicleIndex}, 0};
  }

  // Continue searches from end of line,
  // but only if search does not distance out before that.
  if (searchPoint > m_length)
  {
    // Perform backward search on interfering lines,
    // for yielding for that traffic.
    for (std::vector<Line*>::const_iterator interferingLineIt = m_interfering.cbegin();
        interferingLineIt != m_interfering.cend(); ++interferingLineIt)
    {
      if (*interferingLineIt == requestingLine)
      {
        continue;
      }

      VehicleInfo vehicleAtEndOfLine = *requestingVehicle;
      vehicleAtEndOfLine.position -= m_length;
      SpeedActionInfo yieldingResult =
        (*interferingLineIt)->backwardYieldGetSpeedAction(&vehicleAtEndOfLine, requestingLine);

      if (yieldingResult.speedAction < result.speedAction)
      {
        result = yieldingResult;
      }
    }

    // Perform backward search on cooperating lines,
    // for mutual merging with that traffic.
    for (std::vector<Line*>::const_iterator cooperatingLineIt = m_cooperating.cbegin();
        cooperatingLineIt != m_cooperating.cend(); ++cooperatingLineIt)
    {
      // Skip the line containing the vehicle itself.
      // Otherwise it would brake in order not to collide with itsef...
      if (*cooperatingLineIt == requestingLine)
      {
        continue;
      }

      VehicleInfo vehicleAtEndOfLine = *requestingVehicle;
      vehicleAtEndOfLine.position -= m_length;
      SpeedActionInfo mergingResult =
          (*cooperatingLineIt)->backwardMergeGetSpeedAction(&vehicleAtEndOfLine, requestingLine);

      if (mergingResult.speedAction < result.speedAction)
      {
        result = mergingResult;
      }
    }

    // Perform forward search along the route
    Line * outboundLine = requestingVehicle->getNextRoutePoint(this);
    if (outboundLine != NULL
        && outboundLine != requestingLine)
    {
      VehicleInfo vehicleAtEndOfLine = *requestingVehicle;
      vehicleAtEndOfLine.position -= m_length;
      SpeedActionInfo outboundResult =
          outboundLine->forwardGetSpeedAction(&vehicleAtEndOfLine, requestingLine);

      if (outboundResult.speedAction < result.speedAction)
      {
        result = outboundResult;
      }
    }
  }

  return result;
}

/*
 * Backward search for whether a vehicle may accelerate or must brake
 *
 * requestingVehicle.distance relative to the end of this line
 */

SpeedActionInfo Line::backwardMergeGetSpeedAction(VehicleInfo *requestingVehicle,
                                                  Line        *requestingLine)
{
  // The search should have started with the requesting line,
  // so if we get back to the requesting line we can safely
  // return the "best case" action.
  if (requestingLine == this)
  {
    return {INCREASE, {NULL, 0, -1}, 0};
  }

  // Default to increase speed.
  SpeedActionInfo result = {INCREASE, {NULL, 0, -1}, 0};

  // Adjust position so that the requesting line is relative to
  // the beginning of this line, not the end.
  requestingVehicle->position += m_length;

  if (requestingVehicle->position > m_length)
  {
    // The corresponding position is after the end of this line.
    return {INCREASE, {NULL, 0, -1}, 0};
  }
  else if (requestingVehicle->position < 0)
  {
    // The corresponding position is before the beginning of this line.
    // First, check with all inbound lines.
    for (std::vector<Line*>::const_iterator inboundLineIt = m_in.cbegin();
          inboundLineIt != m_in.cend(); ++inboundLineIt)
    {
      SpeedActionInfo nestedResult
        = (*inboundLineIt)->backwardMergeGetSpeedAction(requestingVehicle,
                                                        requestingLine);

      if (nestedResult.speedAction < result.speedAction)
      {
        result = nestedResult;
      }
    }

    //  Second, check with this blocker, as it might be relevant
    if (!_vehicles.NOW().empty())
    {
      int brakePoint = calculateBrakePoint(requestingVehicle);
      int nextVehicleDistance = _blocker.NOW().distance;
      int nextBrakePoint = calculateBrakePoint(&_blocker.NOW());
      int nextVehicleIndex = _blocker.NOW().index;

      if ((brakePoint + requestingVehicle->speed + VEHICLE_LENGTH) >= nextBrakePoint)
      {
        // Must brake in order not to risk colliding
        return {BRAKE, {this, nextVehicleDistance + (VEHICLE_LENGTH / 2), nextVehicleIndex}, 0};
      }
      else if ((brakePoint + requestingVehicle->speed + SPEEDUP_ACCELERATION + VEHICLE_LENGTH)
          >= std::max(0, nextBrakePoint - BRAKE_ACCELERATION))
      {
        // May not safely increase the speed
        result = {MAINTAIN, {this, nextVehicleDistance + (VEHICLE_LENGTH / 2), nextVehicleIndex}, 0};
      }
    }
  }
  else
  {
    // The corresponding position is on this line.
    // Find and act on the first vehicle after the corresponding position.
    for (std::vector<VehicleInfo>::const_reverse_iterator vehicleIt = _vehicles.NOW().crbegin();
        vehicleIt != _vehicles.NOW().crend(); ++vehicleIt)
    {
      if (vehicleIt->position > requestingVehicle->position)
      {
        int brakePoint = calculateBrakePoint(requestingVehicle);
        int nextVehicleDistance = vehicleIt->position;
        int nextBrakePoint = calculateBrakePoint(&*vehicleIt);
        int nextVehicleIndex = std::distance(_vehicles.NOW().crbegin(), vehicleIt);

        if ((brakePoint + requestingVehicle->speed + VEHICLE_LENGTH) >= nextBrakePoint)
        {
          // Must brake in order not to risk colliding
          return {BRAKE, {this, nextVehicleDistance + (VEHICLE_LENGTH / 2), nextVehicleIndex}, 0};
        }
        else if ((brakePoint + requestingVehicle->speed + SPEEDUP_ACCELERATION + VEHICLE_LENGTH)
            >= std::max(0, nextBrakePoint - BRAKE_ACCELERATION))
        {
          // May not safely increase the speed
          result = {MAINTAIN, {this, nextVehicleDistance + (VEHICLE_LENGTH / 2), nextVehicleIndex}, 0};
        }

        break; // We are finished, as we found and handled the closest vehicle.
      }
    }
  }

  return result;
}

SpeedActionInfo Line::backwardYieldGetSpeedAction(VehicleInfo *requestingVehicle,
                                                  Line        *requestingLine)
{
  bool yield = true;

  // The search should have started with the requesting line,
  // so if we get back to the requesting line we can safely
  // return the "best case" action.
  if (requestingLine == this)
  {
    return {INCREASE, {NULL, 0, -1}, 0};
  }

  // Default to increase speed.
  SpeedActionInfo result = {INCREASE, {NULL, 0, -1}, 0};

  // Adjust position so that the requesting line is relative to
  // the beginning of this line, not the end.
  requestingVehicle->position += m_length;

  if (requestingVehicle->position > m_length)
  {
    // The corresponding position is after the end of this line.
    // For yielding behaviour there might be blockers here.
    if (yield && requestingVehicle->position
        <= m_length + SPEED + (pow(SPEED, 2) / (2 * BRAKE_ACCELERATION)))
    {
      // If the front vehicle in this line has a brake point further along
      // than requstingVehicle.distance, then return BRAKE
      if (!_vehicles.NOW().empty())
      {
        int behindVehicleDistance = _vehicles.NOW()[0].position;
        int behindBrakePoint = calculateBrakePoint(&_vehicles.NOW()[0]);

        if ((behindBrakePoint + _vehicles.NOW()[0].speed + VEHICLE_LENGTH)
            >= requestingVehicle->position)
        {
          if (_vehicles.NOW()[0].vehiclesToYieldFor.find(requestingVehicle->id)
              == _vehicles.NOW()[0].vehiclesToYieldFor.end())
          {
            return {BRAKE, {this, behindVehicleDistance + (VEHICLE_LENGTH / 2), 0}, 0};
          }
          else
          {
            std::cout << "Giving right of way to a yielding vehicle." << std::endl;
          }
        }
      }

      // Continue looping recursively, return BRAKE at once if found.
      for (std::vector<Line*>::const_iterator inboundLineIt = m_in.cbegin();
            inboundLineIt != m_in.cend(); ++inboundLineIt)
      {
        SpeedActionInfo nestedResult
          = (*inboundLineIt)->backwardYieldGetSpeedAction(requestingVehicle,
                                                          requestingLine);

        if (nestedResult.speedAction < result.speedAction)
        {
          result = nestedResult;
        }
      }
    }
  }
  else if (requestingVehicle->position < 0)
  {
    // The corresponding position is before the beginning of this line.
    // First, check with all inbound lines.
    for (std::vector<Line*>::const_iterator inboundLineIt = m_in.cbegin();
          inboundLineIt != m_in.cend(); ++inboundLineIt)
    {
      SpeedActionInfo nestedResult
        = (*inboundLineIt)->backwardYieldGetSpeedAction(requestingVehicle,
                                                        requestingLine);

      if (nestedResult.speedAction < result.speedAction)
      {
        result = nestedResult;
      }
    }

    //  Second, check with this blocker, as it might be relevant
    if (!_vehicles.NOW().empty())
    {
      int brakePoint = calculateBrakePoint(requestingVehicle);
      int nextVehicleDistance = _blocker.NOW().distance;
      int nextBrakePoint = calculateBrakePoint(&_blocker.NOW());
      int nextVehicleIndex = _blocker.NOW().index;

      if ((brakePoint + requestingVehicle->speed + VEHICLE_LENGTH) >= nextBrakePoint)
      {
        // Must brake in order not to risk colliding
        return {BRAKE, {this, nextVehicleDistance + (VEHICLE_LENGTH / 2), nextVehicleIndex}, 0};
      }
      else if ((brakePoint + requestingVehicle->speed + SPEEDUP_ACCELERATION + VEHICLE_LENGTH)
          >= std::max(0, nextBrakePoint - BRAKE_ACCELERATION))
      {
        // May not safely increase the speed
        result = {MAINTAIN, {this, nextVehicleDistance + (VEHICLE_LENGTH / 2), nextVehicleIndex}, 0};
      }
    }
  }
  else
  {
    // The corresponding position is on this line.
    // Find and act on the first vehicle after the corresponding position.
    for (std::vector<VehicleInfo>::const_reverse_iterator vehicleIt = _vehicles.NOW().crbegin();
        vehicleIt != _vehicles.NOW().crend(); ++vehicleIt)
    {
      // FIXME WTF is the purpose and meaning of this if block?
      // It looks like yielding for an incoming car which is further back,
      // but the "3 times max speed behind" logic seems strange. Is there
      // another calculation that would make more sense?
      if (vehicleIt->position + (3 * SPEED)
              >= requestingVehicle->position)
      {
        int nextVehicleDistance = vehicleIt->position;
        int nextVehicleIndex = std::distance(_vehicles.NOW().crbegin(), vehicleIt);
        // FIXME It looks like the direction of iterating through vehicles
        //       means that the culprit the furthest back will trigger
        //       this BRAKE action, when a vehicle further ahead will also
        //       be a valid culprit. This results in a longer gridlock loop
        //       has to be detected than if the first culprit in the line
        //       got registered instead of the last.
        return {BRAKE, {this, nextVehicleDistance, nextVehicleIndex}, 0};
      }

      if (vehicleIt->position > requestingVehicle->position)
      {
        int brakePoint = calculateBrakePoint(requestingVehicle);
        int nextVehicleDistance = vehicleIt->position;
        int nextBrakePoint = calculateBrakePoint(&*vehicleIt);
        int nextVehicleIndex = std::distance(_vehicles.NOW().crbegin(), vehicleIt);

        if ((brakePoint + requestingVehicle->speed + VEHICLE_LENGTH) >= nextBrakePoint)
        {
          // Must brake in order not to risk colliding
          return {BRAKE, {this, nextVehicleDistance + (VEHICLE_LENGTH / 2), nextVehicleIndex}, 0};
        }
        else if ((brakePoint + requestingVehicle->speed + SPEEDUP_ACCELERATION + VEHICLE_LENGTH)
            >= std::max(0, nextBrakePoint - BRAKE_ACCELERATION))
        {
          // May not safely increase the speed
          result = {MAINTAIN, {this, nextVehicleDistance + (VEHICLE_LENGTH / 2), nextVehicleIndex}, 0};
        }

        // If yielding, see if the previous vehicle is also too close
        if (yield)
        {
          if (vehicleIt == _vehicles.NOW().crbegin())
          {
            // The vehicle "in front of" is the last vehicle in this line,
            // so the vehicle "behind" is in an incoming line.
            for (std::vector<Line*>::const_iterator inboundLineIt = m_in.cbegin();
                  inboundLineIt != m_in.cend(); ++inboundLineIt)
            {
              SpeedActionInfo nestedResult
                = (*inboundLineIt)->backwardYieldGetSpeedAction(requestingVehicle,
                                                                requestingLine);

              if (nestedResult.speedAction < result.speedAction)
              {
                result = nestedResult;
              }
            }
          }
          else
          {
            // The vehicle "behind" is in this line.
            std::vector<VehicleInfo>::const_reverse_iterator vehicleBehindIt = vehicleIt - 1;

            int behindVehicleDistance = vehicleBehindIt->position;
            int behindBrakePoint = calculateBrakePoint(&*vehicleBehindIt);
            int nextVehicleIndex = std::distance(_vehicles.NOW().crbegin(), vehicleBehindIt);

            if ((behindBrakePoint + vehicleBehindIt->speed + VEHICLE_LENGTH)
                >= requestingVehicle->position)
            {
              // The other vehicle may have to brake for us. We can not have that.
              return {BRAKE, {this, behindVehicleDistance + (VEHICLE_LENGTH / 2), nextVehicleIndex}, 0};
            }
          }
        }

        break; // We are finished, as we found and handled the closest vehicle.
      }
    }
  }

  return result;
}

void Line::addIn(Line * in)
{
  if (std::find(m_in.begin(), m_in.end(), in) == m_in.end())
  {
    m_in.push_back(in);
    in->addOut(this);
  }
}

void Line::addOut(Line * out)
{
  if (std::find(m_out.begin(), m_out.end(), out) == m_out.end())
  {
    m_out.push_back(out);
    out->addIn(this);
  }
}

void Line::addCooperating(Line * cooperating)
{
  if (std::find(m_cooperating.begin(), m_cooperating.end(), cooperating) == m_cooperating.end())
  {
    m_cooperating.push_back(cooperating);
  }
}

void Line::addInterfering(Line * interfering)
{
  if (std::find(m_interfering.begin(), m_interfering.end(), interfering) == m_interfering.end())
  {
    m_interfering.push_back(interfering);
  }
}

std::vector<Line *> Line::getOut()
{
  return m_out;
}

int Line::getLength()
{
  return m_length;
}

void Line::tick(int tickType)
{
  switch(tickType)
  {
    case 0:
      tick0();
      break;
    case 1:
      tick1();
      break;
    default:
      // Unknown tick type, do not tick.
      break;
  }
}

void Line::tick0()
{
  // Start with blank sheets
  _vehicles.THEN().clear();
  _packets.THEN().clear();

#if 0
  if (!_packets.NOW().empty())
  {
    std::cout << "Packets on line " << this << ": ";
    for (std::vector<unsigned int>::const_iterator it = _packets.NOW().cbegin();
        it != _packets.NOW().cend(); ++it)
    {
//      int packetIndex = std::distance(_packets.NOW().cbegin(), it);
      int packetNumber = *it;
      std::cout << packetNumber << ", ";
    }
    std::cout << std::endl;
  }
#endif

  // Move all the packets
  for (std::vector<unsigned int>::const_iterator it = _packets.NOW().cbegin();
      it != _packets.NOW().cend(); ++it)
  {
    TransportNetworkPacket * packet = p_transportNetwork->getPacket(*it);
    int packetIndex = std::distance(_packets.NOW().cbegin(), it);
    int distance = packet->mutableData.NOW().positionAtLine;

    SpeedActionInfo nextSpeedActionInfo = forwardGetSpeedAction(packet, packetIndex, this, distance);

    SpeedAction previousAction = packet->mutableData.NOW().speedAction;
    SpeedAction nextAction = nextSpeedActionInfo.speedAction;

    int previousSpeed = packet->mutableData.NOW().speed;
    int nextSpeed = previousSpeed;

    switch (nextAction)
    {
      case BRAKE:
        nextSpeed -= BRAKE_ACCELERATION;
        if (nextSpeed < 0)
        {
          nextSpeed = 0;
        }
        break;
      case INCREASE:
        nextSpeed += SPEEDUP_ACCELERATION;
        if (nextSpeed > packet->preferredSpeed)
        {
          nextSpeed = packet->preferredSpeed;
        }
        break;
      default:
        break;
    }

    packet->mutableData.THEN() = packet->mutableData.NOW();

    packet->mutableData.THEN().waitingFor = nextSpeedActionInfo.blockedBy;
    packet->mutableData.THEN().physicallyBlocked = nextSpeedActionInfo.physicallyBlocked;
    packet->mutableData.THEN().line = this;

    packet->mutableData.THEN().speedAction = nextAction;
    packet->mutableData.THEN().speed = nextSpeed;
    int nextPosition = distance + nextSpeed;
    packet->mutableData.THEN().positionAtLine = nextPosition;

    if (nextPosition < m_length)
    {
      _packets.THEN().push_back(*it);
    }
    else
    {
      // Move overflowing vehicles to next line
      packet->mutableData.THEN().positionAtLine -= m_length;
      // Choose out line to put the vehicle based on vehicle route.
      if (!m_out.empty())
      {
        Line * nextLine = packet->mutableData.NOW().getNextRoutePoint(this);
        if (!nextLine)
        {
          if (!m_out.empty())
          {
            nextLine = m_out[rand() % m_out.size()];
          }
          else
          {
            std::cerr << "No route found for packet" << *it
                      << " of line " << this << std::endl;
            return;
          }
        }
        nextLine->deliverPacket(this, *it);
      }
    }
  }

#if 0
  // Move all the vehicles
  for (std::vector<VehicleInfo>::const_iterator it = _vehicles.NOW().cbegin();
      it != _vehicles.NOW().cend(); ++it)
  {
    int vehicleIndex = std::distance(_vehicles.NOW().cbegin(), it);

    VehicleInfo newVehicle = *it;

    newVehicle.speedActionInfo = forwardGetSpeedAction(&newVehicle, this, vehicleIndex);

    SpeedAction previousAction = it->speedActionInfo.speedAction;
    SpeedAction thisAction = newVehicle.speedActionInfo.speedAction;

    // Count the time passed in the same action.
    if (previousAction == thisAction)
    {
      newVehicle.speedActionInfo.timeSinceLastActionChange = it->speedActionInfo.timeSinceLastActionChange + 1;
    }

    // Vehicle has been stopped for some time. Are we gridlocked?
    if (newVehicle.speed == 0
        && (thisAction == BRAKE || thisAction == MAINTAIN)
        && newVehicle.speedActionInfo.culprit.line != this
        && newVehicle.speedActionInfo.timeSinceLastActionChange >= 2)
    {
      // We have already checked that we are at a complete stand-still,
      // that we are indeed at an intersection (waiting for a separate line),
      // and that some time has passed.
      //
      // We may consider chekcing if the culprit is in our direct forward path,
      // but that is not completely thought through yet and may be a mistake.
      // It may simply be too restrictive.
      //
      // We may want to limit our search depth as well, but for now waiting
      // for a hard coded magic number of ticks before starting the search
      // will probably suffice.

      SpeedActionInfo comingFromSpeedActionInfo = newVehicle.speedActionInfo;
      SpeedActionInfo goingToSpeedActionInfo;
      int thisVehicleId = newVehicle.id;
      int comingFromId = thisVehicleId;
      int longestWaitCandidateId = thisVehicleId;
      int longestWaitCandidateTime = newVehicle.speedActionInfo.timeSinceLastActionChange;
      std::set<int> visitedVehicleIds;

      for (int timeout = newVehicle.speedActionInfo.timeSinceLastActionChange;
          timeout > 0; --timeout)
      {
        // Break if not waiting
        if (comingFromSpeedActionInfo.culprit.line == NULL)
        {
          break;
        }

        // FIXME WTF? Why does this even happen?
        if (comingFromSpeedActionInfo.culprit.line->getVehicle(comingFromSpeedActionInfo.culprit.index) == NULL)
        {
          std::cout
            << "Invalid index ("
            << comingFromSpeedActionInfo.culprit.index
            << ") for culprit."
            << std::endl;
          break;
        }

        VehicleInfo goingToVehicleInfo = *comingFromSpeedActionInfo.culprit.line->getVehicle(comingFromSpeedActionInfo.culprit.index);
        goingToSpeedActionInfo = goingToVehicleInfo.speedActionInfo;

        // Break if not waiting
        if (goingToSpeedActionInfo.speedAction == INCREASE)
        {
          break;
        }

        // Check if looping (but not back to here), if so break
        int goingToId = goingToVehicleInfo.id;
        if (visitedVehicleIds.count(goingToId))
        {
          break;
        }

        // Keep track of the longest waiting vehicle.
        // Use ID for tie breaker.
        int goingToTime = goingToSpeedActionInfo.timeSinceLastActionChange;
        if ((goingToTime > longestWaitCandidateTime)
            || (goingToTime == longestWaitCandidateTime
              && goingToId < longestWaitCandidateId))
        {
          longestWaitCandidateTime = goingToTime;
          longestWaitCandidateId = goingToId;
        }

        // If not back here at this point, continue
        if (goingToId != thisVehicleId)
        {
          comingFromId = goingToId;
          comingFromSpeedActionInfo = goingToSpeedActionInfo;
          continue;
        }

        else if (comingFromId == longestWaitCandidateId)
        {
          std::cout << "Solvable loop detected for ID " << comingFromId << std::endl;
          newVehicle.vehiclesToYieldFor.insert(comingFromId);
          break;
        }

        // If we fall through all the way it means we did return to the
        // original vehicle, but not from the longest waiter. In any case
        // we are finished.
        break;
      }

      // TODO Use "vehiclesToYieldFor" in the backwards searches: Return
      // INCREASE when the requestingVehicle is in the vehiclesToYieldFor list
      // of the vehicle it should otherwise BRAKE or MAINTAIN for. (Not to be
      // implemented here; implement in the appropriate searches.)
    }

    // Delete all extraordinary yielding when the vehicle can move again,
    // as that means the gridlock has now been solved.
    if (thisAction != previousAction
        && thisAction == INCREASE
        && newVehicle.vehiclesToYieldFor.size())
    {
      newVehicle.vehiclesToYieldFor.clear();
    }

    switch (thisAction)
    {
      case BRAKE:
        newVehicle.speed -= BRAKE_ACCELERATION;
        if (newVehicle.speed < 0)
        {
          newVehicle.speed = 0;
        }
        break;
      case INCREASE:
        newVehicle.speed += SPEEDUP_ACCELERATION;
        if (newVehicle.speed > newVehicle.preferredSpeed)
        {
          newVehicle.speed = newVehicle.preferredSpeed;
        }
        break;
      default:
        break;
    }

    newVehicle.position += newVehicle.speed;
    
    if (newVehicle.position < m_length)
    {
      _vehicles.THEN().push_back(newVehicle);
    }
    else
    {
      // Move overflowing vehicles to next line
      newVehicle.position -= m_length;
      // Choose out line to put the vehicle based on vehicle route.
      if (!m_out.empty())
      {
        Line * nextLine = newVehicle.getNextRoutePoint(this);
        if (!nextLine)
        {
          if (!m_out.empty())
          {
            nextLine = m_out[rand() % m_out.size()];
          }
          else
          {
            std::cerr << "No route found for vehicle" << &newVehicle
                      << " of line " << this << std::endl;
            return;
          }
        }
        nextLine->deliverVehicle(this, newVehicle);
      }
    }
  }
#endif
}

void Line::tick1()
{
  // Fetch all incoming packets from inboxes, in sorted order
  std::map<Line*, std::vector<unsigned int> >::iterator lineInFrontIt = m_packetInboxes.begin();
  while (lineInFrontIt != m_packetInboxes.end())
  {
    for (std::map<Line*, std::vector<unsigned int> >::iterator lineIt
        = m_packetInboxes.begin(); lineIt != m_packetInboxes.end(); lineIt++)
    {
      if (lineIt->second.empty())
      {
        continue;
      }
      if (lineInFrontIt->second.empty())
      {
        lineInFrontIt = lineIt;
      }
      else
      {
        int packetPosition = p_transportNetwork->getPacket(*lineIt->second.rbegin())->mutableData.THEN().positionAtLine;
        int frontPacketPosition = p_transportNetwork->getPacket(*lineInFrontIt->second.rbegin())->mutableData.THEN().positionAtLine;
        if (packetPosition > frontPacketPosition)
        {
          lineInFrontIt = lineIt;
        }
      }
    }
    
    if (lineInFrontIt != m_packetInboxes.end() && !lineInFrontIt->second.empty())
    {
//      std::cout << "Fetched packet #" << *lineInFrontIt->second.rbegin() << " for line " << this << std::endl;
      addPacket(*lineInFrontIt->second.rbegin());
      lineInFrontIt->second.pop_back();
    }
    else
    {
      break;
    }
  }

  // Fetch all incoming vehicles from inboxes, in sorted order
  std::map<Line*, std::vector<VehicleInfo> >::iterator lineInFrontIt2 = m_vehicleInboxes.begin();
  while (lineInFrontIt2 != m_vehicleInboxes.end())
  {
    for (std::map<Line*, std::vector<VehicleInfo> >::iterator lineIt
        = m_vehicleInboxes.begin(); lineIt != m_vehicleInboxes.end(); lineIt++)
    {
      if (lineIt->second.empty())
      {
        continue;
      }
      if (lineInFrontIt2->second.empty())
      {
        lineInFrontIt2 = lineIt;
      }
      else if (lineIt->second.rbegin()->position > lineInFrontIt2->second.rbegin()->position)
      {
        lineInFrontIt2 = lineIt;
      }
    }
    
    if (lineInFrontIt2 != m_vehicleInboxes.end() && !lineInFrontIt2->second.empty())
    {
      addVehicle(*lineInFrontIt2->second.rbegin());
      lineInFrontIt2->second.pop_back();
    }
    else
    {
      break;
    }
  }

  // Update blocker
  if (!_vehicles.THEN().empty())
  {
    std::vector<VehicleInfo>::const_reverse_iterator lastVehicle = _vehicles.THEN().crbegin();
    _blocker.THEN().distance = lastVehicle->position - VEHICLE_LENGTH;
    _blocker.THEN().speed = lastVehicle->speed;
  }

  // Just a basic test, counting tick number.
  tickNumber.THEN() = tickNumber.NOW() + 1;
}

void Line::print()
{
  std::cout << "Line state:" << std::endl;

  for (std::vector<VehicleInfo>::const_iterator it = _vehicles.NOW().cbegin();
      it != _vehicles.NOW().cend(); ++it)
  {
    std::cout << "position: "
              << it->position
              << ", speed: "
              << it->speed
              << std::endl;
  }

  std::cout << "Tick number: " << tickNumber.NOW();
  std::cout << std::endl;
}

void Line::draw()
{
  // Draw the line
  glLineWidth(1.5);
  float red = 0.8;
  float green = 0.8;
  float blue = 1.0;
  if (!m_interfering.empty())
  {
    red = 1.0;
    blue = 0.8;
  }
  if (!m_cooperating.empty())
  {
    green = 1.0;
    blue = 0.8;
  }
  glColor3f(red, green, blue);

  glBegin(GL_LINES);
  glVertex3f(m_beginPoint.x, m_beginPoint.y, m_beginPoint.z);
  glVertex3f(m_endPoint.x, m_endPoint.y, m_endPoint.z);
  glEnd();

  float angle = std::atan2(m_endPoint.y - m_beginPoint.y, m_endPoint.x - m_beginPoint.x);

  // Draw vehicles / traffic network packets
  const float SCALED_VEHICLE_HEIGHT = static_cast<float>(VEHICLE_HEIGHT) / ZOOM_FACTOR;
  const float HALF_VEHICLE_LENGTH = static_cast<float>(VEHICLE_LENGTH) / 2.0f / ZOOM_FACTOR;
  const float HALF_VEHICLE_WIDTH = static_cast<float>(VEHICLE_WIDTH) / 2.0f / ZOOM_FACTOR;

  // Draw based on VehicleInfo (old, to be removed)
  for (std::vector<VehicleInfo>::const_iterator it = _vehicles.NOW().cbegin();
      it != _vehicles.NOW().cend(); ++it)
  {
    // Calculate vehicle coordinates
    Coordinates vehicleCoordinates = coordinatesFromLineDistance(it->position);

    // Draw vehicle
    glPushMatrix();
    glColor3f(it->color[0], it->color[1], it->color[2]);
    glTranslatef(vehicleCoordinates.x, vehicleCoordinates.y, vehicleCoordinates.z);
    glRotatef(angle * 180 / M_PI, 0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, 0.0f, SCALED_VEHICLE_HEIGHT);
    glVertex3f( HALF_VEHICLE_LENGTH,  HALF_VEHICLE_WIDTH, 0.0f);
    glVertex3f(-HALF_VEHICLE_LENGTH,  HALF_VEHICLE_WIDTH, 0.0f);
    glVertex3f(-HALF_VEHICLE_LENGTH, -HALF_VEHICLE_WIDTH, 0.0f);
    glVertex3f( HALF_VEHICLE_LENGTH, -HALF_VEHICLE_WIDTH, 0.0f);
    glVertex3f( HALF_VEHICLE_LENGTH,  HALF_VEHICLE_WIDTH, 0.0f);
    glEnd();
    glPopMatrix();

    switch (it->speedActionInfo.speedAction)
    {
      case BRAKE:
        glColor3f(1.0f, 0.5f, 0.5f);
        break;
      case MAINTAIN:
        glColor3f(1.0f, 1.0f, 0.5f);
        break;
      case INCREASE:
        glColor3f(0.5f, 1.0f, 0.5f);
        break;
      default:
        break;
    }

    if (it->speedActionInfo.culprit.line != NULL)
    {
      Coordinates culprit = it->speedActionInfo.culprit.line->coordinatesFromLineDistance(it->speedActionInfo.culprit.distance);
      glBegin(GL_LINES);
      glVertex3f(vehicleCoordinates.x, vehicleCoordinates.y, vehicleCoordinates.z + SCALED_VEHICLE_HEIGHT);
      glVertex3f(culprit.x, culprit.y, culprit.z);
      glEnd();
    }
  }

  // Failsafe, although it should never happen. TODO Return error code?
  if (p_transportNetwork == NULL)
  {
    return;
  }

  // Draw based on transport network packets (new, keep)
  for (std::vector<unsigned int>::const_iterator it = _packets.NOW().cbegin();
      it != _packets.NOW().cend(); ++it)
  {
    TransportNetworkPacket * packet = p_transportNetwork->getPacket(*it);
    if (packet == NULL)
    {
      continue;
    }
    TransportNetworkPacketMutableData const * mutableData = &packet->mutableData.NOW();

    Coordinates vehicleCoordinates = coordinatesFromLineDistance(mutableData->positionAtLine);

    Vehicle const * vehicle = packet->vehicle;
    if (vehicle == 0)
    {
      vehicle = &DEFAULT_VEHICLE;
    }

    // Draw packet
    glPushMatrix();
    glColor3f(vehicle->color[0], vehicle->color[1], vehicle->color[2]);
    glTranslatef(vehicleCoordinates.x, vehicleCoordinates.y, vehicleCoordinates.z);
    glRotatef(angle * 180 / M_PI, 0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, 0.0f, SCALED_VEHICLE_HEIGHT);
    glVertex3f( HALF_VEHICLE_LENGTH,  HALF_VEHICLE_WIDTH, 0.0f);
    glVertex3f(-HALF_VEHICLE_LENGTH,  HALF_VEHICLE_WIDTH, 0.0f);
    glVertex3f(-HALF_VEHICLE_LENGTH, -HALF_VEHICLE_WIDTH, 0.0f);
    glVertex3f( HALF_VEHICLE_LENGTH, -HALF_VEHICLE_WIDTH, 0.0f);
    glVertex3f( HALF_VEHICLE_LENGTH,  HALF_VEHICLE_WIDTH, 0.0f);
    glEnd();

    // Draw lines towards the packet that this packet is waiting for
    switch (packet->mutableData.NOW().speedAction)
    {
      case BRAKE:
        glColor3f(1.0f, 0.5f, 0.5f);
        break;
      case MAINTAIN:
        glColor3f(1.0f, 1.0f, 0.5f);
        break;
      case INCREASE:
        glColor3f(0.5f, 1.0f, 0.5f);
        break;
      default:
        break;
    }

    float pointerSize = SCALED_VEHICLE_HEIGHT * 0.25f;

    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, 0.0f, SCALED_VEHICLE_HEIGHT * 3);
    glVertex3f( pointerSize,  pointerSize, SCALED_VEHICLE_HEIGHT * 2);
    glVertex3f(-pointerSize,  pointerSize, SCALED_VEHICLE_HEIGHT * 2);
    glVertex3f(-pointerSize, -pointerSize, SCALED_VEHICLE_HEIGHT * 2);
    glVertex3f( pointerSize, -pointerSize, SCALED_VEHICLE_HEIGHT * 2);
    glVertex3f( pointerSize,  pointerSize, SCALED_VEHICLE_HEIGHT * 2);
    glEnd();
    glPopMatrix();

    TransportNetworkPacket * culpritPacket = p_transportNetwork->getPacket(packet->mutableData.NOW().waitingFor);
    if (culpritPacket != NULL && culpritPacket->mutableData.NOW().line != NULL)
    {
      Coordinates culprit = culpritPacket->mutableData.NOW().line->coordinatesFromLineDistance(culpritPacket->mutableData.NOW().positionAtLine);

      glBegin(GL_LINES);
      glVertex3f(vehicleCoordinates.x, vehicleCoordinates.y, vehicleCoordinates.z + SCALED_VEHICLE_HEIGHT);
      glVertex3f(culprit.x, culprit.y, culprit.z);
      glEnd();
    }

  }

}

std::vector<VehicleInfo> Line::getVehicles()
{
  return _vehicles.NOW();
}

VehicleInfo const * Line::getVehicle(int index) const
{
  if (index >= 0 && static_cast<unsigned int>(index) < _vehicles.NOW().size())
  {
    return &_vehicles.NOW()[index];
  }

  return NULL;
}

void Line::moveRight(float distance)
{
  // Move line slightly to its right, to separate two-way traffic
  Coordinates unitVector;
  unitVector.x = (m_endPoint.x - m_beginPoint.x) * ZOOM_FACTOR / m_length;
  unitVector.y = (m_endPoint.y - m_beginPoint.y) * ZOOM_FACTOR / m_length;
  unitVector.z = 0.0f;

  Coordinates normalVector;
  normalVector.x = unitVector.y;
  normalVector.y = -unitVector.x;
  normalVector.z = 0.0f;

  m_beginPoint.x += (distance * normalVector.x);
  m_beginPoint.y += (distance * normalVector.y);
  m_endPoint.x += (distance * normalVector.x);
  m_endPoint.y += (distance * normalVector.y);
}


Coordinates Line::coordinatesFromVehicleIndex(int index)
{
  return coordinatesFromLineDistance(_vehicles.NOW()[index].position);
}

Coordinates Line::coordinatesFromLineDistance(int distance)
{
  Coordinates coords;
  coords.x = m_beginPoint.x + (distance * (m_endPoint.x - m_beginPoint.x) / m_length);
  coords.y = m_beginPoint.y + (distance * (m_endPoint.y - m_beginPoint.y) / m_length);
  coords.z = m_beginPoint.z + (distance * (m_endPoint.z - m_beginPoint.z) / m_length);
  return coords;
}

