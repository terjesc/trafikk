
#include "line.h"

#include <climits>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <GL/glew.h>

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
  int numberOfVehicles = m_length / AVERAGE_ROAD_LENGTH_PER_VEHICLE;
  int roadFractionLeft = m_length % AVERAGE_ROAD_LENGTH_PER_VEHICLE;
  if (rand() % AVERAGE_ROAD_LENGTH_PER_VEHICLE < roadFractionLeft)
  {
    numberOfVehicles += 1;
  }
  numberOfVehicles = rand() % (1 + (2 * numberOfVehicles));

  std::vector<VehicleInfo> v;
  for (int i = 0; i < numberOfVehicles; ++i)
  {
    int id = totalNumberOfVehicles + i + 1;
    int preferedSpeed = SPEED - 1000 + (rand() % 2001);
    int position = m_length - (i * m_length / numberOfVehicles);
    SpeedActionInfo speedActionInfo = {INCREASE, {NULL, 0}, 0};
    VehicleInfo vi = {id, SPEED, preferedSpeed, position, {0}, std::deque<Line*>(), speedActionInfo};
    vi.color[0] = 0.4f + ((rand() % 50) / 100.0f);
    vi.color[1] = 0.4f + ((rand() % 50) / 100.0f);
    vi.color[2] = 0.4f + ((rand() % 50) / 100.0f);
    v.push_back(vi);
  }
  totalNumberOfVehicles += numberOfVehicles;
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
          return {BRAKE, {this, behindVehicleDistance + (VEHICLE_LENGTH / 2), 0}, 0};
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

  // Move all the vehicles
  for (std::vector<VehicleInfo>::const_iterator it = _vehicles.NOW().cbegin();
      it != _vehicles.NOW().cend(); ++it)
  {
    int vehicleIndex = std::distance(_vehicles.NOW().cbegin(), it);

    VehicleInfo newVehicle = *it;

    newVehicle.speedActionInfo = forwardGetSpeedAction(&newVehicle, this, vehicleIndex);

    SpeedAction previousAction = it->speedActionInfo.speedAction;
    SpeedAction thisAction = newVehicle.speedActionInfo.speedAction;

    // Vehicle has been stopped for some time. Are we gridlocked?
    if (newVehicle.speed == 0
        && thisAction == BRAKE
        && newVehicle.speedActionInfo.culprit.line != this
        && newVehicle.speedActionInfo.timeSinceLastActionChange >= 5)
    {
      // TODO We must know which vehicle is the culprit, in order to iterate
      //      through the chain of blocking vehicles. Now we only have the
      //      distance relative to the line it is currently inhabiting.

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

      // TODO Detect loop, by following path of culprits until:
      //
      //      A) A vehicle is reached which is not waiting for another vehicle.
      //      B) A loop is detected, but this vehicle is not in the loop.
      //      C) We get back to this vehicle.
      //
      //      In case of A or B, not a loop, wait before trying again.
      //      * Possibly by resetting timeSinceLastActionChange?
      //      * Renaming that variable to better reflect its use?
      //      * Store it not as part of the speedActionInfo, but rather
      //        in the vehicleInfo structure itself, as that seems a more
      //        reasonable place to have it?
      //
      //      In case of C, further checking is necessary:
      //      D) The one waiting for this vehicle, has been waiting the longest
      //         in the entire loop. (Tie-breaker: Vehicle ID)
      //
      //      In case of D, add D to "vehiclesToYieldFor".
      //
      //      TODO Use "vehiclesToYieldFor" in the backwards searches:
      //           Return INCREASE when the requestingVehicle is in the
      //           vehiclesToYieldFor list of the vehicle it should otherwise
      //           BRAKE or MAINTAIN for.
    }

    // Delete all extraordinary yielding when the vehicle can move again,
    // as that means the gridlock has now been solved.
    if (thisAction != previousAction
        && thisAction == INCREASE
        && newVehicle.vehiclesToYieldFor.size())
    {
      newVehicle.vehiclesToYieldFor.clear();
    }

    // Count the time passed in the same action.
    if (previousAction == thisAction)
    {
      newVehicle.speedActionInfo.timeSinceLastActionChange = it->speedActionInfo.timeSinceLastActionChange + 1;
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

    switch (it->speedActionInfo.speedAction)
    {
      case BRAKE:
        glColor3f(1.0f, 0.5f, 0.5f);
        break;
      case MAINTAIN:
        glColor3f(1.0f, 1.0f, 0.5f);
        break;
      default:
        break;
    }

    if (it->speedActionInfo.culprit.line != NULL)
    {
      Coordinates culprit = it->speedActionInfo.culprit.line->coordinatesFromLineDistance(it->speedActionInfo.culprit.distance);
      glBegin(GL_LINES);
      glVertex3f(vehicleCoordinates.x, vehicleCoordinates.y, SCALED_VEHICLE_HEIGHT);
      glVertex3f(culprit.x, culprit.y, culprit.z);
      glEnd();
    }
  }
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


Coordinates Line::coordinatesFromVehicleIndex(int index)
{
  return coordinatesFromLineDistance(_vehicles.NOW()[index].position);
}

Coordinates Line::coordinatesFromLineDistance(int distance)
{
  Coordinates coords;
  coords.x = m_beginPoint.x + (distance * (m_endPoint.x - m_beginPoint.x) / m_length);
  coords.y = m_beginPoint.y + (distance * (m_endPoint.y - m_beginPoint.y) / m_length);
  coords.z = 0.0f;
  return coords;
}

