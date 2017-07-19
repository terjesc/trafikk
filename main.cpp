
#include <GL/glew.h>

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/System/Clock.hpp>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui-SFML.h"

#include <iostream>
//#include <utility>
#include <vector>
#include <map>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <climits>

#include "startup_sound.h"

#include "controller.h"
#include "controlleruser.h"
#include "lockstepvalue.h"

const int SPEED = 50;
const int SAFE_DISTANCE = 500;
const int LINE_LENGTH = 10000;
const int VEHICLE_LENGTH = 500;

const int ZOOM_SHRINKING_FACTOR = 20;

const int AVERAGE_NUMBER_OF_VEHICLES = 7;
const int NUMBER_OF_LINES = 1000;
      // For reference, Cities Skylines has a maximum of 32768 road segments
const int NUMBER_OF_LINES_TO_DRAW = 10;

const bool LIMIT_FRAMERATE = false;

static int totalNumberOfVehicles = 0;

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
    std::map<int, std::vector<VehicleInfo> > m_vehicleInbox;
    
    Coordinates m_beginPoint, m_endPoint;

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
        int vehiclePreferedSpeed = SPEED - 10 + (rand() % 21);
        int vehiclePosition = LINE_LENGTH - (i * LINE_LENGTH / numberOfVehicles);
        VehicleInfo vi = {SPEED, vehiclePreferedSpeed, vehiclePosition};
        v.push_back(vi);
      }
      _vehicles.initialize(v);
      m_out.push_back(this);
      m_in.push_back(this);

      m_beginPoint = { static_cast<float>(rand() % 20) - 10.0f,
                       static_cast<float>(rand() % 20) - 10.0f,
                       0.0f };
      m_endPoint = { static_cast<float>(rand() % 20) - 10.0f,
                     static_cast<float>(rand() % 20) - 10.0f,
                     0.0f };
      m_length = 1000 * sqrt(pow(m_beginPoint.x - m_endPoint.x, 2) + pow(m_beginPoint.y - m_endPoint.y, 2));
    }

    void addVehicle(VehicleInfo vehicleInfo)
    {
      _vehicles.THEN().push_back(vehicleInfo);
    }

    void deliverVehicle(int inboxNumber, VehicleInfo vehicleInfo)
    {
      m_vehicleInbox[inboxNumber].push_back(vehicleInfo);
    }

    Blocker getBlocker(int maxDistance)
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
        else
        {
          Blocker blocker = m_out[0]->getBlocker(maxDistance - m_length);
          blocker.distance = blocker.distance + m_length;
          return blocker;
        }
      }
    }


    int getLength()
    {
      return m_length;
    }

    virtual void tick(int tickType)
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

    void tick0()
    {
      // Start with blank sheets
      _vehicles.THEN().clear();

      // Move all the vehicles
      for (std::vector<VehicleInfo>::const_iterator it = _vehicles.NOW().cbegin();
          it != _vehicles.NOW().cend(); ++it)
      {
        // Find the relative distance to the car in front
        int vehicleIndex = std::distance(_vehicles.NOW().cbegin(), it);
        Blocker nextVehicle;
        
        VehicleInfo element = *it;
        
        if (vehicleIndex == 0)
        {
          nextVehicle = m_out[0]->getBlocker(500);
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

    void draw()
    {
      // Draw the line
      glLineWidth(1.5);
      glColor3f(0.8f, 0.8f, 1.0f);
      glBegin(GL_LINES);
      glVertex3f(m_beginPoint.x, m_beginPoint.y, m_beginPoint.z);
      glVertex3f(m_endPoint.x, m_endPoint.y, m_endPoint.z);
      glEnd();

      // TODO draw vehicles
    }

    std::vector<VehicleInfo> getVehicles()
    {
      return _vehicles.NOW();
    }

};

void unbindModernGL()
{
  // Unbind to allow for SFML graphics
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUseProgram(0);
}


int main()
{
  sf::ContextSettings contextSettings;
  contextSettings.depthBits = 24;
  sf::RenderWindow window(sf::VideoMode(800, 600),
      "Trafikk",
      sf::Style::Default,
      contextSettings);

  // Initialize GLEW
  glewInit();

  // Initialize ImGui
  ImGui::SFML::Init(window);

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

  playStartupSound();
 
  // Load a font
  sf::Font font;
  char fontName[] = "FreeMono.ttf";
  font.loadFromFile(fontName);

  // OpenGL init
  glViewport(0, 0, window.getSize().x, window.getSize().y);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  GLfloat ratio = static_cast<float>( window.getSize().x ) / window.getSize().y;
  glFrustum(-ratio * 0.5f, ratio * 0.5f, -0.5f, 0.5f, 1.0f, 500.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearDepth(1.0f);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);

  GLfloat rtri = 0.0f;
  GLfloat rquad = 0.0f;

  // Controller for ticking and lockstep values
  Controller controller;
  controller.registerTickType(0);
  controller.registerTickType(1);


  Line *lines[NUMBER_OF_LINES];
  for (int i = 0; i < NUMBER_OF_LINES; ++i)
  {
    lines[i] = new Line(&controller);
  }

  std::cout << "Total number of vehicles: "
    << totalNumberOfVehicles << std::endl;

  lines[0]->print();

  sf::Clock deltaClock;
  sf::Clock fpsClock;

  // Main loop
  std::cout << "Entering main loop..." << std::endl;
  bool running = true;
  while (running)
  {
    sf::Event event;
    while (window.pollEvent(event))
    {
      ImGui::SFML::ProcessEvent(event);

      switch (event.type)
      {
        case sf::Event::Closed:
          running = false;
          break;
        default:
          break;
      }
    }

    sf::Time elapsed = deltaClock.restart();

    rtri += 0.0005f * 360 * elapsed.asMilliseconds();
    rquad -= 0.00025 * 360 * elapsed.asMilliseconds();

    glClearColor(0.05f, 0.05f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Camera position
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -25.0f); // Go back (zoom out, fixed)
    glRotatef(-30.0f, 1.0f, 0.0f, 0.0f); // Tilt (fixed)
    glRotatef(-10.0f, 0.0f, 0.0f, 1.0f); // Rotate (fixed)


    // Draw an outline of our sandbox area
    glLineWidth(2.5);
    glColor3f(1.0f, 0.8f, 0.8f);
    glBegin(GL_LINE_STRIP);
    glVertex3f(-10.0f, -10.0f, 0.0f);
    glVertex3f(-10.0f, 10.0f, 0.0f);
    glVertex3f(10.0f, 10.0f, 0.0f);
    glVertex3f(10.0f, -10.0f, 0.0f);
    glVertex3f(-10.0f, -10.0f, 0.0f);
    glEnd();


    // Draw some of the lines, using OpenGL
    int linesToDraw = std::min(NUMBER_OF_LINES, NUMBER_OF_LINES_TO_DRAW);
    for (int i = 0; i < linesToDraw; ++i)
    {
      lines[i]->draw();
    }

    // Prepare for drawing through SFML
    unbindModernGL();
    window.pushGLStates();

    ImGui::SFML::Update(window, elapsed);

//    sf::Time elapsed = fpsClock.restart();
    float fps = 1000.0 / elapsed.asMilliseconds();
    char fps_str[30];
    snprintf( fps_str, 30, "%5.1f FPS", fps);
    char vehicle_str[30];
    snprintf( vehicle_str, 30, "%i vehicles", totalNumberOfVehicles);

    ImGui::Begin("Diagnostics");
    ImGui::Text(fps_str);
    ImGui::Text(vehicle_str);
    ImGui::End();

    controller.tick();

//    window.clear(sf::Color::Black);

    // Draw some of the lines
    
    const int MARKER_RADIUS = 5;
    sf::CircleShape marker(MARKER_RADIUS);
    sf::Text markerText("", font);
    markerText.setFillColor(sf::Color::Red);
    markerText.setCharacterSize(14);
    window.draw(markerText);

    for (int i = 0; i < linesToDraw; ++i)
    {
      int yCoordinate = (2 * (i + 1) * MARKER_RADIUS);
      
      //  Draw line
      sf::Vertex line[] =
      {
        sf::Vertex(sf::Vector2f(10, yCoordinate)),
        sf::Vertex(sf::Vector2f(10 + (lines[i]->getLength() / ZOOM_SHRINKING_FACTOR), yCoordinate))
      };
      window.draw(line, 2, sf::Lines);

      // Draw vehicles
      std::vector<VehicleInfo> linevehicles = lines[i]->getVehicles();
      int vehicleNumber = 1;
      for (std::vector<VehicleInfo>::const_iterator it = linevehicles.cbegin();
          it != linevehicles.cend(); ++it)
      {
        // Circular marker for vehicle
        marker.setPosition(MARKER_RADIUS + (it->position / ZOOM_SHRINKING_FACTOR), yCoordinate - MARKER_RADIUS);
        window.draw(marker);
        // Write the vehicle number on the vehicle
        char markerTextString[4];
        snprintf(markerTextString, 4, "%i", vehicleNumber++);
        markerText.setString(markerTextString);
        markerText.setPosition(MARKER_RADIUS + (it->position / ZOOM_SHRINKING_FACTOR), yCoordinate - (2 * MARKER_RADIUS));
        window.draw(markerText);
      }
    }

    ImGui::SFML::Render(window);

    window.popGLStates();

    window.display();
  }

  lines[0]->print();

  for (int i = 0; i < NUMBER_OF_LINES; ++i)
  {
    delete lines[i];
  }

  ImGui::SFML::Shutdown();
  return 0;
}

