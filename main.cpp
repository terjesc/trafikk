
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

#include "startup_sound.h"

#include "line.h"
#include "controller.h"
#include "controlleruser.h"
#include "lockstepvalue.h"

const int SAFE_DISTANCE = 500;
const int LINE_LENGTH = 10000;

const int ZOOM_SHRINKING_FACTOR = 20;

const int NUMBER_OF_LINES = 1000;
      // For reference, Cities Skylines has a maximum of 32768 road segments
const int NUMBER_OF_LINES_TO_DRAW = 10;

const int NUMBER_OF_NODES = 10;

const bool LIMIT_FRAMERATE = false;

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


  // Make some lines
  Line *lines[NUMBER_OF_LINES];
  for (int i = 0; i < NUMBER_OF_LINES; ++i)
  {
    lines[i] = new Line(&controller);
  }

  // Make some other lines
  std::vector<Line*> otherLines;

  // Make some "nodes", to aid in generating a random network of lines.
  std::cout << "Creating nodes..." << std::endl;
  struct
  {
    Coordinates coordinates;
    std::set<Line*> in;
    std::set<Line*> out;
  } nodes[NUMBER_OF_NODES];

  // Randomize the nodes
  for (int i = 0; i < NUMBER_OF_NODES; ++i)
  {
    nodes[i].coordinates.x = static_cast<float>( rand() % 20 ) - 10.0f;
    nodes[i].coordinates.y = static_cast<float>( rand() % 20 ) - 10.0f;
    nodes[i].coordinates.z = 0.0f;
  }

  // Create a line exiting from every node
  for (int sourceIndex = 0; sourceIndex < NUMBER_OF_NODES; ++sourceIndex)
  {
    int targetIndex = sourceIndex;
    do
    {
      targetIndex = rand() % NUMBER_OF_NODES;
    } while (targetIndex == sourceIndex);

    // Create the new line
    Line* newLine = new Line(&controller, &nodes[sourceIndex].coordinates, &nodes[targetIndex].coordinates);
    otherLines.push_back(newLine);
    nodes[sourceIndex].out.insert(newLine);
    nodes[targetIndex].in.insert(newLine);

    // Set up the connections for the line
    for (std::set<Line*>::const_iterator it = nodes[sourceIndex].in.cbegin();
        it != nodes[sourceIndex].in.cend(); ++it)
    {
      newLine->addIn(*it);
    }
    for (std::set<Line*>::const_iterator it = nodes[targetIndex].out.cbegin();
        it != nodes[targetIndex].out.cend(); ++it)
    {
      newLine->addOut(*it);
    }
  }

  // Create a line entering every node with no entering line thus far
  for (int targetIndex = 0; targetIndex < NUMBER_OF_NODES; ++targetIndex)
  {
    if (!nodes[targetIndex].in.empty())
    {
//      continue;
    }
    int sourceIndex = targetIndex;
    do
    {
      sourceIndex = rand() % NUMBER_OF_NODES;
    } while (sourceIndex == targetIndex);

    // Create the new line
    Line* newLine = new Line(&controller, &nodes[sourceIndex].coordinates, &nodes[targetIndex].coordinates);
    otherLines.push_back(newLine);
    nodes[sourceIndex].out.insert(newLine);
    nodes[targetIndex].in.insert(newLine);

    // Set up the connections for the line
    for (std::set<Line*>::const_iterator it = nodes[sourceIndex].in.cbegin();
        it != nodes[sourceIndex].in.cend(); ++it)
    {
      newLine->addIn(*it);
    }
    for (std::set<Line*>::const_iterator it = nodes[targetIndex].out.cbegin();
        it != nodes[targetIndex].out.cend(); ++it)
    {
      newLine->addOut(*it);
    }
  }

#if DEBUG_PRINTOUT_FOR_THE_NODES 
  // Debug printout for the nodes
  for (int nodeIndex = 0; nodeIndex < NUMBER_OF_NODES; ++nodeIndex)
  {
    std::cout << "Node " << nodeIndex << ": " << std::endl;
    std::cout << "\tIn:" << std::endl;
    for (std::set<Line*>::const_iterator lineIt = nodes[nodeIndex].in.cbegin();
        lineIt != nodes[nodeIndex].in.cend(); ++lineIt)
    {
      std::cout << "\t\t" << *lineIt << std::endl;
    }
    std::cout << "\tOut:" << std::endl;
    for (std::set<Line*>::const_iterator lineIt = nodes[nodeIndex].out.cbegin();
        lineIt != nodes[nodeIndex].out.cend(); ++lineIt)
    {
      std::cout << "\t\t" << *lineIt << std::endl;
    }
  }
#endif

  std::cout << "Total number of vehicles: "
    << Line::totalNumberOfVehicles << std::endl;

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
    for (int i = 0; i < static_cast<int>( otherLines.size() ); ++i)
    {
      otherLines[i]->draw();
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
    snprintf( vehicle_str, 30, "%i vehicles", Line::totalNumberOfVehicles);

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

    for (int i = 0; i < static_cast<int>( otherLines.size() ); ++i)
    {
      int yCoordinate = (2 * (i + 1) * MARKER_RADIUS);
      
      //  Draw line
      sf::Vertex line[] =
      {
        sf::Vertex(sf::Vector2f(10, yCoordinate)),
        sf::Vertex(sf::Vector2f(10 + (otherLines[i]->getLength() / ZOOM_SHRINKING_FACTOR), yCoordinate))
      };
      window.draw(line, 2, sf::Lines);

      // Draw vehicles
      std::vector<VehicleInfo> linevehicles = otherLines[i]->getVehicles();
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

