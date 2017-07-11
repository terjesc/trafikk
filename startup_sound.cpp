#include <unistd.h>
#include <SFML/Audio.hpp>

//Samplerate used in playing and generating sounds
const double SAMPLE_RATE = 44100;

//Magnitude of square wave
const int MAX_SQUARE_WAVE_MAGNITUDE = 5000;

/**
 * Generate a square wave with the given frequency and duration.
 *
 * \param frequency Input frequency in MHz
 * \param duration Duration of signal in seconds
 * \return Square wave sampled at SAMPLE_RATE
 **/
std::vector<sf::Int16> generate_square_wave(double frequency, double duration)
{
  //initialize entire square wave array to low value
  size_t samples = duration*SAMPLE_RATE;
  std::vector<sf::Int16> square_wave(samples, -MAX_SQUARE_WAVE_MAGNITUDE);

  size_t numSamplesInPeriod = SAMPLE_RATE/frequency;
  size_t numPeriods = samples/numSamplesInPeriod;

  //assign square wave array to high value during first half of each period
  for (size_t i = 0; i < numPeriods; i++)
  {
    std::vector<sf::Int16>::iterator start = square_wave.begin() + i*numSamplesInPeriod;
    std::fill_n(start, numSamplesInPeriod/2, MAX_SQUARE_WAVE_MAGNITUDE);
  }
  return square_wave;
}

//Frequency of square wave in startup sound
const double STARTUP_SQUARE_WAVE_FREQUENCY = 200.0;

//Duration of square wave in startup sound
const double STARTUP_SQUARE_WAVE_DURATION = 0.5;

//Number of channels used in square wave in startup sound
const int NUM_STARTUP_SQUARE_WAVE_CHANNELS = 1;

//Number of different startup sounds played in sequence
const int NUM_STARTUP_SOUNDS = 10;

//Pause inbetween each new startup sound played
const double DURATION_BETWEEN_STARTUP_SOUNDS = 0.04e06;

void playStartupSound()
{
  //create base square wave array
  std::vector<sf::Int16> square_wave = generate_square_wave(STARTUP_SQUARE_WAVE_FREQUENCY, STARTUP_SQUARE_WAVE_DURATION);

  //prepare sound from square wave array
  sf::SoundBuffer soundBuffer;
  soundBuffer.loadFromSamples(&square_wave[0], square_wave.size(), NUM_STARTUP_SQUARE_WAVE_CHANNELS, SAMPLE_RATE);

  //play square wave a number of times at increasing pitch
  sf::Sound sound(soundBuffer);
  sound.play();
  for (int i=0; i < NUM_STARTUP_SOUNDS; i++) {
    sound.setPitch(1.0 + i/(NUM_STARTUP_SOUNDS*1.0));
    sound.play();
    usleep(DURATION_BETWEEN_STARTUP_SOUNDS);
  }

  //block until sound has stopped playing
  while (true)
  {
    if (sound.getStatus() == sf::SoundSource::Stopped)
    {
      return;
    }

    usleep(DURATION_BETWEEN_STARTUP_SOUNDS);
  }
}
