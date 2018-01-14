
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
#include <string>
#include <sstream>
#include <fstream>

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

std::vector<Line*> createRandomLines(Controller &controller, bool oneWayLines)
{
  std::vector<Line*> lines;

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

  if (oneWayLines)
  {
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
      lines.push_back(newLine);
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
  }

  // Create a line entering every node
  for (int targetIndex = 0; targetIndex < NUMBER_OF_NODES; ++targetIndex)
  {
    int sourceIndex = targetIndex;
    do
    {
      sourceIndex = rand() % NUMBER_OF_NODES;
    } while (sourceIndex == targetIndex);

    // Create the new line
    Line* newLine = new Line(&controller, &nodes[sourceIndex].coordinates, &nodes[targetIndex].coordinates);
    lines.push_back(newLine);
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

    if (oneWayLines == false)
    {
      // Create a line the other way, for getting two lane roads
      Line* reverseLine = new Line(&controller, &nodes[targetIndex].coordinates, &nodes[sourceIndex].coordinates);
      lines.push_back(reverseLine);
      nodes[targetIndex].out.insert(reverseLine);
      nodes[sourceIndex].in.insert(reverseLine);

      // Set up the connections for the reverse line
      for (std::set<Line*>::const_iterator it = nodes[targetIndex].in.cbegin();
          it != nodes[targetIndex].in.cend(); ++it)
      {
        reverseLine->addIn(*it);
      }
      for (std::set<Line*>::const_iterator it = nodes[sourceIndex].out.cbegin();
          it != nodes[sourceIndex].out.cend(); ++it)
      {
        reverseLine->addOut(*it);
      }
    }
  }

  return lines;
}

std::vector<Line*> createMergeTestLines(Controller &controller, float centerX, float centerY, bool yieldEnable = false)
{
  std::vector<Line*> lines;

  // Make some "nodes", to aid in generating the network.
  std::cout << "Creating nodes..." << std::endl;
  struct
  {
    Coordinates coordinates;
    std::set<Line*> in;
    std::set<Line*> out;
  } nodes[6];

  // Set node coordinates
  nodes[0].coordinates = {centerX, centerY, 0.0f};
  nodes[1].coordinates = {centerX + 8.0f, centerY, 0.0f};
  nodes[2].coordinates = {centerX + 8.0f, centerY + 1.0f, 0.0f};
  nodes[3].coordinates = {centerX - 8.0f, centerY + 1.0f, 0.0f};
  nodes[4].coordinates = {centerX + 8.0f, centerY - 1.0f, 0.0f};
  nodes[5].coordinates = {centerX - 8.0f, centerY - 1.0f, 0.0f};

  int linePaths[] = {0, 1, // sourceNode, targetNode
                     1, 2, // line 1, forking from node 1
                     2, 3,
                     3, 0, // line 3, merging into node 0
                     1, 4, // line 4, forking from node 1
                     4, 5,
                     5, 0}; // line 6, merging into node 0
  const int LINE_COUNT = 7;

  int yieldingLines[] = {6, 3}; // Line 6 should yield for traffic on line 3
  const int YIELD_COUNT = 1;

  int cooperatingLines[] = {3, 6}; // Line 3 should cooperate with traffic on line 6
  const int COOPERATE_COUNT = 1;

  // Set up lines between the nodes
  for (int i = 0; i < LINE_COUNT; ++i)
  {
    // Fetch node indexes for line begin and end points
    int sourceIndex = linePaths[2 * i];
    int targetIndex = linePaths[2 * i + 1];

    // Create line
    Line* newLine = new Line(&controller, nodes[sourceIndex].coordinates,
        nodes[targetIndex].coordinates);

    // Register ins and outs
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

    // Register line at nodes
    nodes[sourceIndex].out.insert(newLine);
    nodes[targetIndex].in.insert(newLine);

    // Push line to return value
    lines.push_back(newLine);
  }

  if (yieldEnable)
  {
    for (int i = 0; i < YIELD_COUNT; ++i)
    {
      lines[yieldingLines[2*i]]->addInterfering(lines[yieldingLines[2*i +1]]);
    }
  }
  else
  {
    for (int i = 0; i < YIELD_COUNT; ++i)
    {
      lines[yieldingLines[2*i]]->addCooperating(lines[yieldingLines[2*i +1]]);
    }
    for (int i = 0; i < COOPERATE_COUNT; ++i)
    {
      lines[cooperatingLines[2*i]]->addCooperating(lines[cooperatingLines[2*i + 1]]);
    }
  }

  return lines;
}

std::vector<Line*> loadTestLines(Controller &controller, std::string fileName)
{
  std::map<int, Line*> lineMap;
  std::vector<std::pair<int, int> > inLines;
  std::vector<std::pair<int, int> > outLines;
  std::vector<std::pair<int, int> > mergeLines;
  std::vector<std::pair<int, int> > yieldLines;

  std::string line;
  std::ifstream lineFile(fileName.c_str());

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

    begin.z -= 30.0f;
    end.z -= 30.0f;

    Line *newLine = new Line(&controller, begin, end);
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

  std::vector<Line*> lines;

  for (std::map<int, Line*>::const_iterator lineIt = lineMap.cbegin();
      lineIt != lineMap.cend(); ++lineIt)
  {
    lines.push_back(lineIt->second);
  }

  return lines;
}


int main()
{
  sf::ContextSettings contextSettings;
  contextSettings.depthBits = 24;
  sf::RenderWindow window(sf::VideoMode(1024, 768),
      "Trafikk",
      sf::Style::Default,
      contextSettings);

  // Initialize GLEW
  glewInit();

  // Initialize ImGui
  ImGui::SFML::Init(window);

  /*
   * If using exponential speed settings from 1 to 60,
   * these are some options:
   *
   *  1 ->  8 -> 60
   *  1 ->  4 -> 15 -> 60
   *  1 ->  3 ->  8 -> 22 -> 60
   *  1 ->  2 ->  5 -> 12 -> 26 -> 60
   */

#define LIMIT_FRAMERATE 15
  // Framerate and sync settings
  if (LIMIT_FRAMERATE)
  {
    window.setVerticalSyncEnabled(false);
    window.setFramerateLimit(LIMIT_FRAMERATE);
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


  // Make a transport network
  TransportNetwork transportNetwork(&controller);
  transportNetwork.loadLinesFromFile("../testbane.txt");

  // Make some lines
//  const bool ONE_WAY_LINES = true;
//  std::vector<Line*> lines = createRandomLines(controller, ONE_WAY_LINES);

  std::vector<Line*> lines;
#if 0
  for (float yPos = 4.0f; yPos < 9.0f; yPos += 4.0f)
  {
    bool yieldEnable = true;
    std::vector<Line*> otherLines = createMergeTestLines(controller, 0.0f, yPos, yieldEnable);

    for (std::vector<Line*>::const_iterator it = otherLines.cbegin();
        it != otherLines.cend(); ++it)
    {
      lines.push_back(*it);
    }
  }
#endif
#if 0
  // Load lines from file
  std::vector<Line*> testLines = loadTestLines(controller, "../testbane.txt");
  for (std::vector<Line*>::const_iterator it = testLines.cbegin();
      it != testLines.cend(); ++it)
  {
    lines.push_back(*it);
  }
#endif
  std::cout << "Total number of vehicles: "
    << Line::totalNumberOfVehicles << std::endl;

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
    glTranslatef(0.0f, 0.0f, -35.0f); // Go back (zoom out, fixed)
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
    for (int i = 0; i < static_cast<int>( lines.size() ); ++i)
    {
      lines[i]->draw();
    }

    // Draw the transportNetwork
    transportNetwork.draw();

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

//    std::cout << "\t-\tTICK\t-\t" << std::endl;
    controller.tick();

    ImGui::SFML::Render(window);

    window.popGLStates();

    window.display();
  }

  ImGui::SFML::Shutdown();
  return 0;
}

