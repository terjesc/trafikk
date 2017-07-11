#pragma once

#include "controlleruser.h"

#include <stdint.h>
#include <set>

const int32_t DEFAULT_TICK = 0;

class ControllerUser;

class Controller
{
  private:
    int m_NOW;
    int m_THEN;

    std::set<ControllerUser*> m_users;
    std::set<int32_t> m_tickTypes;

    void swap();

  public:
    int NOW();
    int THEN();
    
    Controller();

    void registerUser(ControllerUser *user);
    void unregisterUser(ControllerUser *user);
    void registerTickType(int32_t tickType);
    void unregisterTickType(int32_t tickType);

    void tick();
};

