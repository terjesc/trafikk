#pragma once

#include "controller.h"

class Controller;

class ControllerUser
{
  public:
    Controller *controller;
    ControllerUser(Controller *controller);
    virtual ~ControllerUser() {};
    virtual void tick(int tickType) = 0;
};

