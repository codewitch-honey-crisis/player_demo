// Arduino Zero / Feather M0 I2S audio tone generation example.
// Author: Tony DiCola
//
// Connect an I2S DAC or amp (like the UDA1334A) to the Arduino Zero
// and play back simple sine, sawtooth, triangle, and square waves.
// Makes your Zero sound like a NES!
//
// NOTE: The I2S signal generated by the Zero does NOT have a MCLK /
// master clock signal.  You must use an I2S receiver that can operate
// without a MCLK signal (like the UDA1334A).
//
// For an Arduino Zero / Feather M0 connect it to you I2S hardware as follows:
// - Digital 0 -> I2S LRCLK / FS (left/right / frame select clock)
// - Digital 1 -> I2S BCLK / SCLK (bit / serial clock)
// - Digital 9 -> I2S DIN / SD (data output)
// - Ground
//
// Released under a MIT license: https://opensource.org/licenses/MIT


//modified for M5Stack Gray Core

#include <M5Stack.h>
#include <driver/i2s.h>




#define SPEAKER_I2S_NUMBER I2S_NUM_0


#define SAMPLERATE_HZ 44100  // The sample rate of the audio.  Higher sample rates have better fidelity,
                             // but these tones are so simple it won't make a difference.  44.1khz is
                             // standard CD quality sound.

#define AMPLITUDE     ((1<<31)-1)   // Set the amplitude of generated waveforms.  This controls how loud
                             // the signals are, and can be any value from 0 to 2**31 - 1.  Start with
                             // a low value to prevent damaging speakers!

#define WAV_SIZE      256    // The size of each generated waveform.  The larger the size the higher
                             // quality the signal.  A size of 256 is more than enough for these simple
                             // waveforms.


// Define the frequency of music notes (from http://www.phy.mtu.edu/~suits/notefreqs.html):
#define C4_HZ      261.63
#define D4_HZ      293.66
#define E4_HZ      329.63
#define F4_HZ      349.23
#define G4_HZ      392.00
#define A4_HZ      440.00
#define B4_HZ      493.88

// Define a C-major scale to play all the notes up and down.
float scale[] = { C4_HZ, D4_HZ, E4_HZ, F4_HZ, G4_HZ, A4_HZ, B4_HZ, A4_HZ, G4_HZ, F4_HZ, E4_HZ, D4_HZ, C4_HZ };

// Store basic waveforms in memory.
int32_t sine[WAV_SIZE]     = {0};
int32_t sawtooth[WAV_SIZE] = {0};
int32_t triangle[WAV_SIZE] = {0};
int32_t square[WAV_SIZE]   = {0};
size_t writeSize;

// Create I2S audio transmitter object.
//Adafruit_ZeroI2S i2s;

#define Serial Serial

void generateSine(int32_t amplitude, int32_t* buffer, uint16_t length) {
  // Generate a sine wave signal with the provided amplitude and store it in
  // the provided buffer of size length.
  for (int i=0; i<length; ++i) {
    buffer[i] = int32_t(float(amplitude)*sin(2.0*PI*(1.0/length)*i));
  }
}
void generateSawtooth(int32_t amplitude, int32_t* buffer, uint16_t length) {
  // Generate a sawtooth signal that goes from -amplitude/2 to amplitude/2
  // and store it in the provided buffer of size length.
  float delta = float(amplitude)/float(length);
  for (int i=0; i<length; ++i) {
    buffer[i] = -(amplitude/2)+delta*i;
  }
}

void generateTriangle(int32_t amplitude, int32_t* buffer, uint16_t length) {
  // Generate a triangle wave signal with the provided amplitude and store it in
  // the provided buffer of size length.
  float delta = float(amplitude)/float(length);
  for (int i=0; i<length/2; ++i) {
    buffer[i] = -(amplitude/2)+delta*i;
  }
    for (int i=length/2; i<length; ++i) {
    buffer[i] = (amplitude/2)-delta*(i-length/2);
  }
}

void generateSquare(int32_t amplitude, int32_t* buffer, uint16_t length) {
  // Generate a square wave signal with the provided amplitude and store it in
  // the provided buffer of size length.
  for (int i=0; i<length/2; ++i) {
    buffer[i] = -(amplitude/2);
  }
    for (int i=length/2; i<length; ++i) {
    buffer[i] = (amplitude/2);
  }
}

void playWave(int32_t* buffer, uint16_t length, float frequency, float seconds) {
  // Play back the provided waveform buffer for the specified
  // amount of seconds.
  // First calculate how many samples need to play back to run
  // for the desired amount of seconds.
  uint32_t iterations = seconds*SAMPLERATE_HZ;
  // Then calculate the 'speed' at which we move through the wave
  // buffer based on the frequency of the tone being played.
  float delta = (frequency*length)/float(SAMPLERATE_HZ);
  // Now loop through all the samples and play them, calculating the
  // position within the wave buffer for each moment in time.
  for (uint32_t i=0; i<iterations; ++i) {
    uint16_t pos = uint32_t(i*delta) % length;
    int32_t sample = buffer[pos];
    // Duplicate the sample so it's sent to both the left and right channel.
    // It appears the order is right channel, left channel if you want to write
    // stereo sound.
    i2s_write(SPEAKER_I2S_NUMBER, &sample, sizeof(sample), &writeSize, portMAX_DELAY);
  }
}

void setup() {
  // Configure serial port.
  Serial.begin(115200);

  Serial.printf("ESP-IDF Version %d.%d.%d \r\n", ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH);
  Serial.printf("Ardunio Version %d.%d.%d \r\n", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);

  M5.begin(true, true, true, true);

  Serial.println("Audio Tone Generator");


  // Initialize the I2S transmitter.
  esp_err_t err = ESP_OK;

  //i2s_driver_uninstall(SPEAKER_I2S_NUMBER);
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
      .sample_rate = SAMPLERATE_HZ ,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // is fixed at 12bit, stereo, MSB
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 2,
      .dma_buf_len = 256
  };
  
  i2s_config.use_apll = false;
  i2s_config.tx_desc_auto_clear = true;



  err += i2s_driver_install(SPEAKER_I2S_NUMBER, &i2s_config, 0, NULL);
  i2s_pin_config_t tx_pin_config;

  tx_pin_config.mck_io_num = 0;
  tx_pin_config.bck_io_num = 12;
  tx_pin_config.ws_io_num = 13;
  tx_pin_config.data_out_num = 15;
  tx_pin_config.data_in_num = 34;

  err += i2s_set_pin(SPEAKER_I2S_NUMBER, NULL);

  err += i2s_set_clk(SPEAKER_I2S_NUMBER, SAMPLERATE_HZ, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);

  
  

  // Generate waveforms.
  generateSine(AMPLITUDE, sine, WAV_SIZE);
  generateSawtooth(AMPLITUDE, sawtooth, WAV_SIZE);
  generateTriangle(AMPLITUDE, triangle, WAV_SIZE);
  generateSquare(AMPLITUDE, square, WAV_SIZE);
}

void loop() {
  Serial.println("Sine wave");
  for (int i=0; i<sizeof(scale)/sizeof(float); ++i) {
    // Play the note for a quarter of a second.
    playWave(sine, WAV_SIZE, scale[i], 0.25);
    // Pause for a tenth of a second between notes.
    delay(100);
  }
  Serial.println("Sawtooth wave");
  for (int i=0; i<sizeof(scale)/sizeof(float); ++i) {
    // Play the note for a quarter of a second.
    playWave(sawtooth, WAV_SIZE, scale[i], 0.25);
    // Pause for a tenth of a second between notes.
    delay(100);
  }
  Serial.println("Triangle wave");
  for (int i=0; i<sizeof(scale)/sizeof(float); ++i) {
    // Play the note for a quarter of a second.
    playWave(triangle, WAV_SIZE, scale[i], 0.25);
    // Pause for a tenth of a second between notes.
    delay(100);
  }
  Serial.println("Square wave");
  for (int i=0; i<sizeof(scale)/sizeof(float); ++i) {
    // Play the note for a quarter of a second.
    playWave(square, WAV_SIZE, scale[i], 1.0);
    // Pause for a tenth of a second between notes.
    //delay(1);
    delay(100);
  }
