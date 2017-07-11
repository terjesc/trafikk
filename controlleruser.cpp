
#include "controlleruser.h"
#include "controller.h"

ControllerUser::ControllerUser(Controller *controller)
{
  this->controller = controller;
  controller->registerUser(this);
}

