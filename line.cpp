
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
    m_vehicleInboxes[senderLine].push_back(vehicleInfo);
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

/*
 * Forward search for whether a vehicle may accelerate or must brake
 *
 * requestingVehicle.distance relative to this line
 */
SpeedAction Line::forwardGetSpeedAction(Blocker requestingVehicle,
                                        Line*   requestingLine,
                                        int     requestingVehicleIndex)
{
  SpeedAction result = INCREASE;

  int brakeLength = pow(requestingVehicle.speed, 2) / (2 * BRAKE_ACCELERATION);
  int brakePoint = requestingVehicle.distance + brakeLength;
  int searchPoint = brakePoint
                  + (2 * requestingVehicle.speed)
                  + (2 * VEHICLE_LENGTH);

  // Check for vehicles to yield for in this line
  int nextBrakePoint = INT_MAX;
  if (requestingVehicleIndex == -1 // The requesting vehicle is external to this line,
      && !_vehicles.NOW().empty() // and there is a blocker in this line
      && _blocker.NOW().distance < searchPoint) // within search distance
  {
    nextBrakePoint = _blocker.NOW().distance
                   + (pow(_blocker.NOW().speed, 2) / (2 * BRAKE_ACCELERATION));
  }
  else if (requestingVehicleIndex > 0 // The requesting vehicle is in this line (but not first)
      && requestingVehicleIndex < static_cast<int>(_vehicles.NOW().size()))
  {
    nextBrakePoint = _vehicles.NOW()[requestingVehicleIndex - 1].position
                   + (pow(_vehicles.NOW()[requestingVehicleIndex - 1].speed, 2)
                     / (2 * BRAKE_ACCELERATION));
  }

  if ((brakePoint + requestingVehicle.speed + VEHICLE_LENGTH) >= nextBrakePoint)
  {
    // Must brake in order not to risk colliding
    return BRAKE;
  }
  else if ((brakePoint + requestingVehicle.speed + SPEEDUP_ACCELERATION + VEHICLE_LENGTH)
      >= std::max(0, nextBrakePoint - BRAKE_ACCELERATION))
  {
    // May not safely increase the speed
    result = MAINTAIN;
  }

  // Continue searches from end of line,
  // but only if search does not distance out before that.
  if (searchPoint > m_length)
  {
    Blocker vehicleAtEndOfLine = requestingVehicle;
    vehicleAtEndOfLine.distance -= m_length;

    // Perform backward search on interfering lines,
    // for yielding for that traffic.
    for (std::vector<Line*>::const_iterator interferingLineIt = m_interfering.cbegin();
        interferingLineIt != m_interfering.cend(); ++interferingLineIt)
    {
      if (*interferingLineIt == requestingLine)
      {
        continue;
      }

      SpeedAction yieldingResult =
        (*interferingLineIt)->backwardYieldGetSpeedAction(vehicleAtEndOfLine, requestingLine);

      result = std::min(yieldingResult, result);
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

      SpeedAction mergingResult =
          (*cooperatingLineIt)->backwardMergeGetSpeedAction(vehicleAtEndOfLine, requestingLine);

      result = std::min(mergingResult, result);
    }

    // Perform forward search on all outgoing lines.
    for (std::vector<Line*>::const_iterator outboundLineIt = m_out.cbegin();
          outboundLineIt != m_out.cend(); ++outboundLineIt)
    {
      SpeedAction nestedResult =
          (*outboundLineIt)->forwardGetSpeedAction(vehicleAtEndOfLine, requestingLine);

      result = std::min(nestedResult, result);
    }
  }

  return result;
}

/*
 * Backward search for whether a vehicle may accelerate or must brake
 *
 * requestingVehicle.distance relative to the end of this line
 */

SpeedAction Line::backwardMergeGetSpeedAction(Blocker requestingVehicle,
                                              Line*   requestingLine)
{
  // The search should have started with the requesting line,
  // so if we get back to the requesting line we can safely
  // return the "best case" action.
  if (requestingLine == this)
  {
    return INCREASE;
  }

  // Default to increase speed.
  SpeedAction result = INCREASE;

  // Adjust position so that the requesting line is relative to
  // the beginning of this line, not the end.
  requestingVehicle.distance += m_length;

  if (requestingVehicle.distance > m_length)
  {
    // The corresponding position is after the end of this line.
    return INCREASE;
  }
  else if (requestingVehicle.distance < 0)
  {
    // The corresponding position is before the beginning of this line.
    // First, check with all inbound lines.
    for (std::vector<Line*>::const_iterator inboundLineIt = m_in.cbegin();
          inboundLineIt != m_in.cend(); ++inboundLineIt)
    {
      SpeedAction nestedResult
        = (*inboundLineIt)->backwardMergeGetSpeedAction(requestingVehicle,
                                                        requestingLine);

      result = std::min(nestedResult, result);
    }

    //  Second, check with this blocker, as it might be relevant
    if (!_vehicles.NOW().empty())
    {
      int brakeLength = pow(requestingVehicle.speed, 2)
                        / (2 * BRAKE_ACCELERATION);
      int brakePoint = requestingVehicle.distance + brakeLength;
      int nextBrakePoint = _blocker.NOW().distance
                         + (pow(_blocker.NOW().speed, 2)
                           / (2 * BRAKE_ACCELERATION));

      if ((brakePoint + requestingVehicle.speed + VEHICLE_LENGTH) >= nextBrakePoint)
      {
        // Must brake in order not to risk colliding
        return BRAKE;
      }
      else if ((brakePoint + requestingVehicle.speed + SPEEDUP_ACCELERATION + VEHICLE_LENGTH)
          >= std::max(0, nextBrakePoint - BRAKE_ACCELERATION))
      {
        // May not safely increase the speed
        result = MAINTAIN;
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
      if (vehicleIt->position > requestingVehicle.distance)
      {
        int brakeLength = pow(requestingVehicle.speed, 2)
                          / (2 * BRAKE_ACCELERATION);
        int brakePoint = requestingVehicle.distance + brakeLength;
        int nextBrakePoint = vehicleIt->position
                           + (pow(vehicleIt->speed, 2)
                             / (2 * BRAKE_ACCELERATION));

        if ((brakePoint + requestingVehicle.speed + VEHICLE_LENGTH) >= nextBrakePoint)
        {
          // Must brake in order not to risk colliding
          return BRAKE;
        }
        else if ((brakePoint + requestingVehicle.speed + SPEEDUP_ACCELERATION + VEHICLE_LENGTH)
            >= std::max(0, nextBrakePoint - BRAKE_ACCELERATION))
        {
          // May not safely increase the speed
          result = MAINTAIN;
        }

        break; // We are finished, as we found and handled the closest vehicle.
      }
    }
  }

  return result;
}

SpeedAction Line::backwardYieldGetSpeedAction(Blocker requestingVehicle,
                                              Line*   requestingLine)
{
  bool yield = true;

  // The search should have started with the requesting line,
  // so if we get back to the requesting line we can safely
  // return the "best case" action.
  if (requestingLine == this)
  {
    return INCREASE;
  }

  // Default to increase speed.
  SpeedAction result = INCREASE;

  // Adjust position so that the requesting line is relative to
  // the beginning of this line, not the end.
  requestingVehicle.distance += m_length;

  if (requestingVehicle.distance > m_length)
  {
    // The corresponding position is after the end of this line.
    // For yielding behaviour there might be blockers here.
    if (yield && requestingVehicle.distance
        <= m_length + SPEED + (pow(SPEED, 2) / (2 * BRAKE_ACCELERATION)))
    {
      // If the front vehicle in this line has a brake point further along
      // than requstingVehicle.distance, then return BRAKE
      if (!_vehicles.NOW().empty())
      {
        int behindBrakePoint = _vehicles.NOW()[0].position
                             + (pow(_vehicles.NOW()[0].speed, 2)
                               / (2 * BRAKE_ACCELERATION));

        if ((behindBrakePoint + _vehicles.NOW()[0].speed + VEHICLE_LENGTH)
            >= requestingVehicle.distance)
        {
          return BRAKE;
        }
      }

      // Continue looping recursively, return BRAKE at once if found.
      for (std::vector<Line*>::const_iterator inboundLineIt = m_in.cbegin();
            inboundLineIt != m_in.cend(); ++inboundLineIt)
      {
        SpeedAction nestedResult
          = (*inboundLineIt)->backwardYieldGetSpeedAction(requestingVehicle,
                                                          requestingLine);

        result = std::min(nestedResult, result);
      }
    }
  }
  else if (requestingVehicle.distance < 0)
  {
    // The corresponding position is before the beginning of this line.
    // First, check with all inbound lines.
    for (std::vector<Line*>::const_iterator inboundLineIt = m_in.cbegin();
          inboundLineIt != m_in.cend(); ++inboundLineIt)
    {
      SpeedAction nestedResult
        = (*inboundLineIt)->backwardYieldGetSpeedAction(requestingVehicle,
                                                        requestingLine);

      result = std::min(nestedResult, result);
    }

    //  Second, check with this blocker, as it might be relevant
    if (!_vehicles.NOW().empty())
    {
      int brakeLength = pow(requestingVehicle.speed, 2)
                        / (2 * BRAKE_ACCELERATION);
      int brakePoint = requestingVehicle.distance + brakeLength;
      int nextBrakePoint = _blocker.NOW().distance
                         + (pow(_blocker.NOW().speed, 2)
                           / (2 * BRAKE_ACCELERATION));

      if ((brakePoint + requestingVehicle.speed + VEHICLE_LENGTH) >= nextBrakePoint)
      {
        // Must brake in order not to risk colliding
        return BRAKE;
      }
      else if ((brakePoint + requestingVehicle.speed + SPEEDUP_ACCELERATION + VEHICLE_LENGTH)
          >= std::max(0, nextBrakePoint - BRAKE_ACCELERATION))
      {
        // May not safely increase the speed
        result = MAINTAIN;
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
      if (vehicleIt->position + (3 * SPEED)
              >= requestingVehicle.distance)
      {
        return BRAKE;
      }

      if (vehicleIt->position > requestingVehicle.distance)
      {
        int brakeLength = pow(requestingVehicle.speed, 2)
                          / (2 * BRAKE_ACCELERATION);
        int brakePoint = requestingVehicle.distance + brakeLength;
        int nextBrakePoint = vehicleIt->position
                           + (pow(vehicleIt->speed, 2)
                             / (2 * BRAKE_ACCELERATION));

        if ((brakePoint + requestingVehicle.speed + VEHICLE_LENGTH) >= nextBrakePoint)
        {
          // Must brake in order not to risk colliding
          return BRAKE;
        }
        else if ((brakePoint + requestingVehicle.speed + SPEEDUP_ACCELERATION + VEHICLE_LENGTH)
            >= std::max(0, nextBrakePoint - BRAKE_ACCELERATION))
        {
          // May not safely increase the speed
          result = MAINTAIN;
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
              SpeedAction nestedResult
                = (*inboundLineIt)->backwardYieldGetSpeedAction(requestingVehicle,
                                                                requestingLine);

              result = std::min(nestedResult, result);
            }
          }
          else
          {
            // The vehicle "behind" is in this line.
            std::vector<VehicleInfo>::const_reverse_iterator vehicleBehindIt = vehicleIt - 1;
            int behindBrakePoint = vehicleBehindIt->position
                                 + (pow(vehicleBehindIt->speed, 2)
                                   / (2 * BRAKE_ACCELERATION));

            if ((behindBrakePoint + vehicleBehindIt->speed + VEHICLE_LENGTH)
                >= requestingVehicle.distance)
            {
              // The other vehicle may have to brake for us. We can not have that.
              return BRAKE;
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
    int vehicleIndex = std::distance(_vehicles.NOW().cbegin(), it);
   
    VehicleInfo newVehicle = *it;

    Blocker vehicle;
    vehicle.speed = newVehicle.speed;
    vehicle.distance = newVehicle.position;
    switch (forwardGetSpeedAction(vehicle, this, vehicleIndex))
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
      // TODO choose out line to put the vehicle based on vehicle route.
      if (!m_out.empty())
      {
        m_out[rand() % m_out.size()]->deliverVehicle(this, newVehicle);
      }
    }
  }
}

void Line::tick1()
{
  // Fetch all incoming vehicles from inboxes, in sorted order
  std::map<Line*, std::vector<VehicleInfo> >::iterator lineInFrontIt = m_vehicleInboxes.begin();
  while (lineInFrontIt != m_vehicleInboxes.end())
  {
    for (std::map<Line*, std::vector<VehicleInfo> >::iterator lineIt
        = m_vehicleInboxes.begin(); lineIt != m_vehicleInboxes.end(); lineIt++)
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
    
    if (lineInFrontIt != m_vehicleInboxes.end() && !lineInFrontIt->second.empty())
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

