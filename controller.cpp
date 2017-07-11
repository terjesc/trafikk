
#include "controller.h"
#include "controlleruser.h"

#include <stdint.h>
#include <utility>

void Controller::swap()
{
  std::swap(m_NOW, m_THEN);
}

int Controller::NOW()
{
  return m_NOW;
}

int Controller::THEN()
{
  return m_THEN;
}
    
Controller::Controller()
{
  m_NOW = 0;
  m_THEN = 1;

  // Default tick.
  // The implication of a default tick is that users can be agnostic
  // to tick types altogether. If more tick types ar registered,
  // then users must take the tick type into account in their respective
  // tick() functions.
  registerTickType(DEFAULT_TICK);
}

void Controller::registerUser(ControllerUser *user)
{
  m_users.insert(user);
}

void Controller::unregisterUser(ControllerUser *user)
{
  m_users.erase(user);
}

void Controller::registerTickType(int32_t tickType)
{
  m_tickTypes.insert(tickType);
}

void Controller::unregisterTickType(int32_t tickType)
{
  m_tickTypes.erase(tickType);
}

void Controller::tick()
{
  // For all tick steps, tick all users.
  for (std::set<int32_t>::iterator it_tickType = m_tickTypes.begin();
      it_tickType != m_tickTypes.end(); ++it_tickType)
  {
    for (std::set<ControllerUser*>::iterator it_user = m_users.begin();
        it_user != m_users.end(); ++it_user)
    {
      (*it_user)->tick(*it_tickType);
    }
  }

  swap();
}

