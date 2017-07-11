
#include <SFML/Graphics.hpp>

#include <iostream>
//#include <utility>
#include <vector>
#include <map>
#include <cstdlib>
#include <ctime>
#include <algorithm>

#include "controller.h"
#include "controlleruser.h"
#include "lockstepvalue.h"

const int SPEED = 50;
const int SAFE_DISTANCE = 500;
const int LINE_LENGTH = 10000;

const int AVERAGE_NUMBER_OF_VEHICLES = 7;

const bool LIMIT_FRAMERATE = false;

static int totalNumberOfVehicles = 0;

struct Blocker
{
  int speed;
  int position;
};


struct VehicleInfo
{
  int speed;
  int position;
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
    std::map<int, std::vector<VehicleInfo> > m_vehicleInbox;

  public:
    Line(Controller *controller)
      : ControllerUser(controller),
        _blocker(controller, {0, 0}),
        _vehicles(controller),
        tickNumber(controller, 0)
    {
      int numberOfVehicles = rand() % (1 + (2 * AVERAGE_NUMBER_OF_VEHICLES));
      totalNumberOfVehicles += numberOfVehicles;
      std::vector<VehicleInfo> v;
      for (int i = 0; i < numberOfVehicles; ++i)
      {
        int vehicleSpeed = SPEED - 10 + (rand() % 21);
        int vehiclePosition = i * LINE_LENGTH / numberOfVehicles;
        VehicleInfo vi = {vehicleSpeed, vehiclePosition};
        v.push_back(vi);
      }
      _vehicles.initialize(v);
      m_out.push_back(this);
      m_in.push_back(this);
      m_length = LINE_LENGTH;
    }

    void addVehicle(VehicleInfo vehicleInfo)
    {
      _vehicles.THEN().push_back(vehicleInfo);
    }

    void deliverVehicle(int inboxNumber, VehicleInfo vehicleInfo)
    {
      m_vehicleInbox[inboxNumber].push_back(vehicleInfo);
    }

    Blocker getBlocker()
    {
      return _blocker.NOW();
    }

    virtual void tick(int tickType)
    {
      if (tickType == 0)
      {
        tick0();
      }
      else if (tickType == 1)
      {
        tick1();
      }
    }

    void tick0()
    {
      // Start with blank sheets
      _vehicles.THEN().clear();

      // Move all the vehicles
      for (std::vector<VehicleInfo>::const_iterator it = _vehicles.NOW().cbegin();
          it != _vehicles.NOW().cend(); ++it)
      {
        VehicleInfo element = *it;
        element.position = element.position + element.speed;
        if (element.position < m_length)
        {
          _vehicles.THEN().push_back(element);
        }
        else
        {
          // Move overflowing vehicles to next line
          element.position -= m_length;
          // FIXME: use the correct inbox id, depending on graph topology
          const int MY_INBOX_ID = 0;
          m_out[0]->deliverVehicle(MY_INBOX_ID, element);
        }
      }
    }

    void tick1()
    {
      // Fetch all incoming vehicles from inbox
      // FIXME: Terminology: "Inbox" is used both for single inbox (it_inbox)
      //        and map of all inboxes (m_vehicleInbox).
      for (std::map<int, std::vector<VehicleInfo> >::iterator it_inbox
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

      // Just a basic test, counting tick number.
      tickNumber.THEN() = tickNumber.NOW() + 1;
    }

    void print()
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

    std::vector<VehicleInfo> getVehicles()
    {
      return _vehicles.NOW();
    }

};


int main()
{
  sf::RenderWindow window(sf::VideoMode(800, 600),
      "Trafikk",
      sf::Style::Default);

  // Framerate and sync settings
  if (LIMIT_FRAMERATE)
  {
    window.setVerticalSyncEnabled(false);
    window.setFramerateLimit(60);
  }
  else
  {
    window.setVerticalSyncEnabled(true);
  }
  
  std::cout << "Hello, world!" << std::endl;
  std::srand(std::time(NULL));
 
  Controller controller;
  controller.registerTickType(0);
  controller.registerTickType(1);

  Line line(&controller);

  // For reference, Cities Skylines has a maximum of 32768 road segments
  const int NUMBER_OF_ADDITIONAL_LINES = 16384;

  Line *lines[NUMBER_OF_ADDITIONAL_LINES];
  for (int i = 0; i < NUMBER_OF_ADDITIONAL_LINES; ++i)
  {
    lines[i] = new Line(&controller);
  }

  std::cout << "Total number of vehicles: "
    << totalNumberOfVehicles << std::endl;

  line.print();

  // Main loop
  std::cout << "Entering main loop..." << std::endl;
  bool running = true;
  while (running)
  {
    sf::Event event;
    while (window.pollEvent(event))
    {
      switch (event.type)
      {
        case sf::Event::Closed:
          running = false;
          break;
        default:
          break;
      }
    }

    controller.tick();

    window.clear(sf::Color::Black);

    // Draw line
    sf::Vertex lineline[] =
    {
      sf::Vertex(sf::Vector2f(10, 10)),
      sf::Vertex(sf::Vector2f(10 + (LINE_LENGTH / 25), 10))
    };
    window.draw(lineline, 2, sf::Lines);
      
    // Draw vehicles on line
    const int MARKER_RADIUS = 5;
    sf::CircleShape marker(MARKER_RADIUS);

    // Draw vehicles
    std::vector<VehicleInfo> linevehicles = line.getVehicles();
    for (std::vector<VehicleInfo>::const_iterator it = linevehicles.cbegin();
        it != linevehicles.cend(); ++it)
    {
      marker.setPosition(MARKER_RADIUS + (it->position / 25), MARKER_RADIUS);
      window.draw(marker);
    }


    // Draw some more lines
    int linesToDraw = std::min(NUMBER_OF_ADDITIONAL_LINES, 50);
    for (int i = 0; i < linesToDraw; ++i)
    {
      int yCoordinate = (2 * (i + 2) * MARKER_RADIUS);
      
      //  Draw line
      sf::Vertex lineline[] =
      {
        sf::Vertex(sf::Vector2f(10, yCoordinate)),
        sf::Vertex(sf::Vector2f(10 + (LINE_LENGTH / 25), yCoordinate))
      };
      window.draw(lineline, 2, sf::Lines);

      // Draw vehicles
      std::vector<VehicleInfo> linevehicles = lines[i]->getVehicles();
      for (std::vector<VehicleInfo>::const_iterator it = linevehicles.cbegin();
          it != linevehicles.cend(); ++it)
      {
        marker.setPosition(MARKER_RADIUS + (it->position / 25), yCoordinate - MARKER_RADIUS);
        window.draw(marker);
      }

    }

    window.display();
  }

  line.print();

  for (int i = 0; i < NUMBER_OF_ADDITIONAL_LINES; ++i)
  {
    delete lines[i];
  }

  return 0;
}

