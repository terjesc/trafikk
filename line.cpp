
#include "line.h"

#include <climits>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <GL/glew.h>

Line::Line(Controller *controller, Coordinates* beginPoint, Coordinates* endPoint)
  : ControllerUser(controller),
    _blocker(controller, {0, 0}),
    _vehicles(controller),
    tickNumber(controller, 0)
{
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
  int numberOfVehicles = rand() % (1 + (2 * AVERAGE_NUMBER_OF_VEHICLES));
  totalNumberOfVehicles += numberOfVehicles;
  std::vector<VehicleInfo> v;
  for (int i = 0; i < numberOfVehicles; ++i)
  {
    int vehiclePreferedSpeed = SPEED - 1000 + (rand() % 2001);
    int vehiclePosition = m_length - (i * m_length / numberOfVehicles);
    VehicleInfo vi = {SPEED, vehiclePreferedSpeed, vehiclePosition};
    vi.color[0] = 0.4f + ((rand() % 50) / 100.0f);
    vi.color[1] = 0.4f + ((rand() % 50) / 100.0f);
    vi.color[2] = 0.4f + ((rand() % 50) / 100.0f);
    v.push_back(vi);
  }
  _vehicles.initialize(v);
}

Line::Line(Controller *controller, Coordinates beginPoint, Coordinates endPoint)
  : Line(controller, &beginPoint, &endPoint)
{
}

int Line::totalNumberOfVehicles = 0;

void Line::addVehicle(VehicleInfo vehicleInfo)
{
  _vehicles.THEN().push_back(vehicleInfo);
}

void Line::deliverVehicle(Line * senderLine, VehicleInfo vehicleInfo)
{
  if (vehicleInfo.position < m_length)
  {
    m_vehicleInbox[senderLine].push_back(vehicleInfo);
  }
  else
  {
    vehicleInfo.position -= m_length;
    if (!m_out.empty())
    {
      m_out[rand() % m_out.size()]->deliverVehicle(senderLine, vehicleInfo);
    }
  }
}

Blocker Line::getBlocker(int maxDistance)
{

  if (!_vehicles.NOW().empty()) // We have a blocker in this line
  {
    if (_blocker.NOW().distance > maxDistance) // But too far down the road
    {
      Blocker infiniteBlocker = {INT_MAX,INT_MAX};
      return  infiniteBlocker;
    }
    else
    {
      return _blocker.NOW();
    }
  }
  else // No blocker in this line
  {
    if (m_length > maxDistance) // The search "times out" in this line
    {
      Blocker infiniteBlocker = {INT_MAX,INT_MAX};
      return infiniteBlocker;
    }
    else if (m_out.empty()) // No outbound lines for continuing the search
    {
      Blocker infiniteBlocker = {INT_MAX,INT_MAX};
      return infiniteBlocker;
    }
    else // Continue the search on outbound lines
    {
      // Get blockers from all outbound lines
      std::vector<Blocker> blockers;
      for (std::vector<Line*>::const_iterator outboundLineIt = m_out.cbegin();
          outboundLineIt != m_out.cend(); ++outboundLineIt)
      {
        blockers.push_back((*outboundLineIt)->getBlocker(maxDistance - m_length));
      }

      // Choose the closest blocker
      // TODO There may be a better choice, taking speed into account as well
      Blocker blocker = {INT_MAX,INT_MAX};
      for (std::vector<Blocker>::const_iterator blockerIt = blockers.cbegin();
          blockerIt != blockers.cend(); ++blockerIt)
      {
        if (blockerIt->distance < blocker.distance)
        {
          blocker = *blockerIt;
        }
      }

      // Final calculation
      if (blocker.distance <= INT_MAX - m_length)
      {
        blocker.distance = blocker.distance + m_length;
      }
      return blocker;
    }
  }
}

/*
 * Forward search for whether a vehicle may accelerate or must brake
 *
 * requestingVehicle.position relative to this line
 */
SpeedAction Line::forwardGetSpeedAction(Blocker requestingVehicle)
{
  SpeedAction result = INCREASE;
  int brakeLength = pow(requestingVehicle.speed, 2) / (2 * BRAKE_ACCELERATION);
  int brakePoint = requestingVehicle.distance + brakeLength;
  int searchPoint = brakePoint + (2 * requestingVehicle.speed);

  // Check for vehicles to yield for in this line
  if (!_vehicles.NOW().empty()) // We have a blocker in this line
  {
    if (_blocker.NOW().distance < searchPoint) // And it is within search distance
    {
      // Calculate action based on the blocker
      int nextBrakePoint = _blocker.NOW().distance + (pow(_blocker.NOW().speed, 2) / (2 * BRAKE_ACCELERATION));

      if (brakePoint + requestingVehicle.speed >= nextBrakePoint)
      {
        // Must brake in order not to risk colliding
        return BRAKE;
      }
      else if ((brakePoint + requestingVehicle.speed + SPEEDUP_ACCELERATION)
          >= std::max(0, nextBrakePoint - BRAKE_ACCELERATION))
      {
        // May not safely increase the speed
        result = MAINTAIN;
      }
    }
  }

  // TODO: Perform backward search on incoming lines,
  // according to yield status.

  // Provided that the search shall go on,
  // perform forward search on all outgoing lines.
  if (searchPoint > m_length)
  {
    Blocker vehicleToPassOn = requestingVehicle;
    vehicleToPassOn.distance -= m_length;

    for (std::vector<Line*>::const_iterator outboundLineIt = m_out.cbegin();
          outboundLineIt != m_out.cend(); ++outboundLineIt)
    {
      SpeedAction nestedResult = (*outboundLineIt)->forwardGetSpeedAction(vehicleToPassOn);

      if (nestedResult == BRAKE)
      {
        return BRAKE;
      }
      else if (nestedResult == MAINTAIN)
      {
        result = MAINTAIN;
      }
    }
  }

  return result;
}

/*
 * Backward search for whether a vehicle may accelerate or must brake
 *
 * requestingVehicle.position relative to this line
 */
SpeedAction Line::backwardGetSpeedAction(Blocker requestingVehicle)
{
  return BRAKE;
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

  // Move all the vehicles
  for (std::vector<VehicleInfo>::const_iterator it = _vehicles.NOW().cbegin();
      it != _vehicles.NOW().cend(); ++it)
  {
    VehicleInfo element = *it;
    int vehicleBrakePoint = pow(element.speed, 2) / (2 * BRAKE_ACCELERATION);
    
    // Find the relative distance to the car in front
    int vehicleIndex = std::distance(_vehicles.NOW().cbegin(), it);
    Blocker nextVehicle = {INT_MAX,INT_MAX};
    
    if (vehicleIndex == 0)
    {
      if (!m_out.empty())
      {
        nextVehicle = m_out[0]->getBlocker(vehicleBrakePoint + (2 * element.speed));
      }
      if (nextVehicle.distance != INT_MAX)
      {
        nextVehicle.distance += (m_length - element.position);
      }
    }
    else
    {
      nextVehicle.speed = _vehicles.NOW()[vehicleIndex - 1].speed;
      nextVehicle.distance = _vehicles.NOW()[vehicleIndex - 1].position - element.position - VEHICLE_LENGTH;
    }

    int nextBrakePoint;
    
    if (nextVehicle.distance == INT_MAX)
    {
      nextBrakePoint = INT_MAX;
    }
    else
    {
      nextBrakePoint = nextVehicle.distance + (pow(nextVehicle.speed, 2) / (2 * BRAKE_ACCELERATION));
    }

    if (vehicleBrakePoint + element.speed >= nextBrakePoint && element.speed > 0)
    {
      // We may collide! Brake!
      element.speed -= BRAKE_ACCELERATION;
      if (element.speed < 0)
      {
        element.speed = 0;
      }
    }
    else if ((vehicleBrakePoint + element.speed + SPEEDUP_ACCELERATION)
        < std::max(0, nextBrakePoint - BRAKE_ACCELERATION))
    {
      // Next vehicle is far away! We may increase our speed if we want to.
      if (element.speed < element.preferredSpeed)
      {
        element.speed += SPEEDUP_ACCELERATION;
        if (element.speed > element.preferredSpeed)
        {
          element.speed = element.preferredSpeed;
        }
      }
    }
    else if (element.speed > element.preferredSpeed)
    {
      // We go faster than we want to! Brake!
      element.speed -= BRAKE_ACCELERATION;
    }


    // TODO: We may need some sort of addressing/indexing of the vehicles,
    //       for an easy way to find nearby vehicles.

    element.position = element.position + element.speed;
    
    if (element.position < m_length)
    {
      _vehicles.THEN().push_back(element);
    }
    else
    {
      // Move overflowing vehicles to next line
      element.position -= m_length;
      // TODO choose out line to put the vehicle,
      //      currently random.
      if (!m_out.empty())
      {
        // TODO Vehicle delivery should choose recursively down the path,
        //      for the situations where the vehicle is to "skip"
        //      one or more following (too short) lines.
        m_out[rand() % m_out.size()]->deliverVehicle(this, element);
      }
    }
  }
}

void Line::tick1()
{
  // Fetch all incoming vehicles from inbox
  // FIXME Terminology: "Inbox" is used both for single inbox (it_inbox)
  //       and map of all inboxes (m_vehicleInbox).
  
  // Pick incoming vehicles in sorted order
  std::map<Line*, std::vector<VehicleInfo> >::iterator lineInFrontIt = m_vehicleInbox.begin();
  while (lineInFrontIt != m_vehicleInbox.end())
  {
    for (std::map<Line*, std::vector<VehicleInfo> >::iterator lineIt
        = m_vehicleInbox.begin(); lineIt != m_vehicleInbox.end(); lineIt++)
    {
      if (lineIt->second.empty())
      {
        continue;
      }
      if (lineInFrontIt->second.empty())
      {
        lineInFrontIt = lineIt;
      }
      else if (lineIt->second.rbegin()->position > lineInFrontIt->second.rbegin()->position)
      {
        lineInFrontIt = lineIt;
      }
    }
    
    if (lineInFrontIt != m_vehicleInbox.end() && !lineInFrontIt->second.empty())
    {
      addVehicle(*lineInFrontIt->second.rbegin());
      lineInFrontIt->second.pop_back();
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
  glColor3f(0.8f, 0.8f, 1.0f);
  glBegin(GL_LINES);
  glVertex3f(m_beginPoint.x, m_beginPoint.y, m_beginPoint.z);
  glVertex3f(m_endPoint.x, m_endPoint.y, m_endPoint.z);
  glEnd();

  float angle = std::atan2(m_endPoint.y - m_beginPoint.y, m_endPoint.x - m_beginPoint.x);
  // Draw vehicles
  const float SCALED_VEHICLE_HEIGHT = static_cast<float>(VEHICLE_HEIGHT) / ZOOM_FACTOR;
  const float HALF_VEHICLE_LENGTH = static_cast<float>(VEHICLE_LENGTH) / 2.0f / ZOOM_FACTOR;
  const float HALF_VEHICLE_WIDTH = static_cast<float>(VEHICLE_WIDTH) / 2.0f / ZOOM_FACTOR;
  for (std::vector<VehicleInfo>::const_iterator it = _vehicles.NOW().cbegin();
      it != _vehicles.NOW().cend(); ++it)
  {
    // Calculate vehicle coordinates
    Coordinates vehicleCoordinates;
    vehicleCoordinates.x = m_beginPoint.x + (it->position * (m_endPoint.x - m_beginPoint.x) / m_length);
    vehicleCoordinates.y = m_beginPoint.y + (it->position * (m_endPoint.y - m_beginPoint.y) / m_length);
    vehicleCoordinates.z = 0.0f;
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
  }
 //
}

std::vector<VehicleInfo> Line::getVehicles()
{
  return _vehicles.NOW();
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

