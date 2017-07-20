
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

  m_length = 1000 * sqrt(pow(m_beginPoint.x - m_endPoint.x, 2) + pow(m_beginPoint.y - m_endPoint.y, 2));

  // Move line slightly to its right, to separate two-way traffic
  Coordinates unitVector;
  unitVector.x = (m_endPoint.x - m_beginPoint.x) * 1000 / m_length;
  unitVector.y = (m_endPoint.y - m_beginPoint.y) * 1000 / m_length;
  unitVector.z = 0.0f;

  Coordinates normalVector;
  normalVector.x = unitVector.y;
  normalVector.y = -unitVector.x;
  normalVector.z = 0.0f;

  m_beginPoint.x += (0.2f * normalVector.x);
  m_beginPoint.y += (0.2f * normalVector.y);
  m_endPoint.x += (0.2f * normalVector.x);
  m_endPoint.y += (0.2f * normalVector.y);

  int numberOfVehicles = rand() % (1 + (2 * AVERAGE_NUMBER_OF_VEHICLES));
  totalNumberOfVehicles += numberOfVehicles;
  std::vector<VehicleInfo> v;
  for (int i = 0; i < numberOfVehicles; ++i)
  {
    int vehiclePreferedSpeed = SPEED - 10 + (rand() % 21);
    int vehiclePosition = m_length - (i * m_length / numberOfVehicles);
    VehicleInfo vi = {SPEED, vehiclePreferedSpeed, vehiclePosition};
    v.push_back(vi);
  }
  _vehicles.initialize(v);
//  m_out.push_back(this);
//  m_in.push_back(this);
}

int Line::totalNumberOfVehicles = 0;

void Line::addVehicle(VehicleInfo vehicleInfo)
{
  _vehicles.THEN().push_back(vehicleInfo);
}

void Line::deliverVehicle(Line * senderLine, VehicleInfo vehicleInfo)
{
  m_vehicleInbox[senderLine].push_back(vehicleInfo);
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
    if (m_length > maxDistance)
    {
      Blocker infiniteBlocker = {INT_MAX,INT_MAX};
      return infiniteBlocker;
    }
    else if (m_out.empty())
    {
      Blocker infiniteBlocker = {INT_MAX,INT_MAX};
      return infiniteBlocker;
    }
    else
    {
      Blocker blocker = m_out[0]->getBlocker(maxDistance - m_length);
      blocker.distance = blocker.distance + m_length;
      return blocker;
    }
  }
}

void Line::addIn(Line * in)
{
  if (std::find(m_in.begin(), m_in.end(), in) == m_in.end())
  {
    std::cout << "addIn: Connecting line " << in << " to line " << this << std::endl;
    m_in.push_back(in);
    in->addOut(this);
  }
}

void Line::addOut(Line * out)
{
  if (std::find(m_out.begin(), m_out.end(), out) == m_out.end())
  {
    std::cout << "addOut: Connecting line " << this << " to line " << out << std::endl;
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
    // Find the relative distance to the car in front
    int vehicleIndex = std::distance(_vehicles.NOW().cbegin(), it);
    Blocker nextVehicle = {INT_MAX,INT_MAX};
    
    VehicleInfo element = *it;
    
    if (vehicleIndex == 0)
    {
      if (!m_out.empty())
      {
        nextVehicle = m_out[0]->getBlocker(500);
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

    int myBrakePoint = element.speed * element.speed / 2;
    int nextBrakePoint = nextVehicle.distance + (nextVehicle.speed * nextVehicle.speed / 2);

    if (myBrakePoint >= nextBrakePoint && element.speed > 0)
    {
      // We may collide! Brake!
      element.speed -= 1;
    }
    else if ((myBrakePoint + (2 * element.speed) + 1) < (nextBrakePoint - 1))
    {
      // Next vehicle is far away! We may increase our speed if we want to.
      if (element.speed < element.preferedSpeed)
      {
        element.speed += 1;
      }
    }
    else if (element.speed > element.preferedSpeed)
    {
      // We go faster than we want to! Brake!
      element.speed -= 1;
    }


    // TODO: We need some sort of addressing/indexing of the vehicles,
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
      //      currently hard coded to the first slot.
      if (!m_out.empty())
      {
        m_out[rand() % m_out.size()]->deliverVehicle(this, element);
      }
    }
  }
}

void Line::tick1()
{
  // Fetch all incoming vehicles from inbox
  // FIXME: Terminology: "Inbox" is used both for single inbox (it_inbox)
  //        and map of all inboxes (m_vehicleInbox).
  for (std::map<Line*, std::vector<VehicleInfo> >::iterator it_inbox
        = m_vehicleInbox.begin();
      it_inbox != m_vehicleInbox.end(); ++it_inbox)
  {
    for (std::vector<VehicleInfo>::iterator it_element = it_inbox->second.begin();
        it_element != it_inbox->second.end(); ++it_element)
    {
      addVehicle(*it_element);
    }
    it_inbox->second.clear();
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

  // Draw vehicles
  for (std::vector<VehicleInfo>::const_iterator it = _vehicles.NOW().cbegin();
      it != _vehicles.NOW().cend(); ++it)
  {
    // Calculate vehicle coordinates
    Coordinates vehicleCoordinates;
    vehicleCoordinates.x = m_beginPoint.x + (it->position * (m_endPoint.x - m_beginPoint.x) / m_length);
    vehicleCoordinates.y = m_beginPoint.y + (it->position * (m_endPoint.y - m_beginPoint.y) / m_length);
    vehicleCoordinates.z = 0.0f;
    // Draw vehicle
    glColor3f(0.8f, 1.0f, 0.8f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(vehicleCoordinates.x, vehicleCoordinates.y, vehicleCoordinates.z + 1.0f);
    glVertex3f(vehicleCoordinates.x + 0.1f, vehicleCoordinates.y + 0.1f, vehicleCoordinates.z);
    glVertex3f(vehicleCoordinates.x - 0.1f, vehicleCoordinates.y + 0.1f, vehicleCoordinates.z);
    glVertex3f(vehicleCoordinates.x - 0.1f, vehicleCoordinates.y - 0.1f, vehicleCoordinates.z);
    glVertex3f(vehicleCoordinates.x + 0.1f, vehicleCoordinates.y - 0.1f, vehicleCoordinates.z);
    glVertex3f(vehicleCoordinates.x + 0.1f, vehicleCoordinates.y + 0.1f, vehicleCoordinates.z);
    glEnd();
  }
 //
}

std::vector<VehicleInfo> Line::getVehicles()
{
  return _vehicles.NOW();
}

