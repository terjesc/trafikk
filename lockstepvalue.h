#pragma once

#include "controller.h"

template<class T>
class LockStepValue
{
  private:
    T _values[2];
    Controller *_controller;

  public:
    LockStepValue(Controller *controller)
    {
      _controller = controller;
    }
    
    LockStepValue(Controller *controller, const T& value)
    {
      _controller = controller;
      _values[0] = value;
      _values[1] = value;
    }

    void initialize(const T& value)
    {
      _values[0] = value;
      _values[1] = value;
    }

    const T& NOW() const
    {
      return _values[_controller->NOW()];
    }

    T& THEN()
    {
      return _values[_controller->THEN()];
    }
    
};


