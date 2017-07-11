
// FIXME: This is early code, it may not be usefull at all...
// FIXME: Early Lane class below...
// FIXME: Missing includes and stuff...

class VehicleVector
{
  private:
    int containerSize;
    int size;

  public:
    VehicleInfo *vehicles;
    
    VehicleVector(int containerSize = 10)
    {
      this->containerSize = containerSize;
      size = 0;
      vehicles = new VehicleInfo[containerSize];
    }
    
    ~VehicleVector()
    {
      delete[] vehicles;
    }
    
    int getContainerSize()
    {
      return containerSize;
    }

    int getSize()
    {
      return size;
    }

    int setSize(int size)
    {
      if (size > containerSize)
      {
        size = containerSize;
      }
      this->size = size;
      return size;
    }

    void setContainerSize(int newContainerSize, bool copyData = true)
    {
      // Make new container
      VehicleInfo *newVehicles = new VehicleInfo[newContainerSize];
      
      // Copy data if we are told to.
      if (copyData == true)
      {
        // In the below loop, i is limited by both old and new containerSize,
        // in order not to get out of bounds either at reading or writing.
        for (int i = 0; (i < containerSize) && (i < newContainerSize); ++i)
        {
          newVehicles[i] = vehicles[i];
        }

        if (size > newContainerSize)
        {
          size = newContainerSize;
        }
      }

      // If not copying data, our used size should be reset to 0.
      if (copyData == false)
      {
        size = 0;
      }

      // Delete the old, use the new.
      delete[] vehicles;
      vehicles = newVehicles;

      // Final bookkeeping.
      containerSize = newContainerSize;
    }
};


class Lane : public ControllerUser
{
  private:
    VehicleVector *vehicles[2];
    LockStepValue<int> testValue;

  public:
    Lane(Controller *controller) : ControllerUser(controller), testValue(controller, 0)
    {
      vehicles[0] = new VehicleVector(10);
      vehicles[1] = new VehicleVector(10);
    }

    ~Lane()
    {
      delete vehicles[0];
      delete vehicles[1];
    }

    VehicleVector* vehiclesNOW()
    {
      return vehicles[controller->NOW()];
    }

    VehicleVector* vehiclesTHEN()
    {
      return vehicles[controller->THEN()];
    }

    virtual void tick()
    {
      // Move all VehicleSlots forward by their speed.
      VehicleVector* vehiclesREAD = vehiclesNOW();
      VehicleVector* vehiclesWRITE = vehiclesTHEN();

      vehiclesWRITE->setSize(vehiclesREAD->getSize());
      for (int i = 0; i < vehiclesREAD->getSize(); ++i)
      {
        vehiclesWRITE->vehicles[i] = vehiclesREAD->vehicles[i];
        vehiclesWRITE->vehicles[i].position += vehiclesWRITE->vehicles[i].speed;
      }

      testValue.THEN() = testValue.NOW() + 1;
    }

    void print()
    {
      VehicleVector* vehiclesREAD = vehiclesNOW();
      std::cout << "Lane state:" << std::endl;
      for (int i = 0; i < vehiclesREAD->getSize(); ++i)
      {
        std::cout << "position: "
                  << vehiclesREAD->vehicles[i].position
                  << ", speed: "
                  << vehiclesREAD->vehicles[i].speed
                  << std::endl;
      }
      std::cout << "also " << testValue.NOW();
      std::cout << std::endl;
    }
};


