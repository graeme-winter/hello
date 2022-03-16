#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <stdio.h>

// i2c definitions

#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9
#define FREQ 400000

#define ADDRESS 0x74

// register definitions - most of these need to be blanked ->
// mark these as NULL as no electrical interface enabled on a
// pico scroll

// FRAME_MODE -> 0x0 to 0x7 value for _write_ to frames 0 to 7
//               higher bits -> 0 so picture mode
// FRAME_NO -> select picture frame to show
// IMAGE_REGISTER -> 144 byte register for bytes for given frame
//                   only 119 visible (assumed to be not first 119)

#define FRAME_MODE 0x0
#define FRAME_NO 0x1
#define NULL0 0x2
#define NULL1 0x3
#define NULL2 0x6
#define SHUTDOWN 0xa
#define IMAGE_REGISTER 0x24
#define COMMAND 0xfd

// general definitions - buffer size of 144 pixels (defined by controller)
#define BUFFER_SIZE 0x90

// dimensions
#define WIDTH 17
#define HEIGHT 7

// helper functions
int write_byte_to_register(uint8_t reg, uint8_t value) {
  uint8_t data[2];

  data[0] = reg;
  data[1] = value;

  i2c_write_blocking(I2C_PORT, ADDRESS, data, 2, false);

  return 0;
}

int write_bytes_to_register(uint8_t reg, uint8_t *values, uint8_t n) {
  uint8_t data[n + 1];

  data[0] = reg;
  for (uint8_t j = 0; j < n; j++) {
    data[j + 1] = values[j];
  }

  i2c_write_blocking(I2C_PORT, ADDRESS, data, n + 1, false);

  return 0;
}

// map "logical" position (following X-window) to electronic position
void map_buffer_to_pixels(uint8_t *in, uint8_t *out) {
  for (uint8_t j = 0; j < BUFFER_SIZE; j++) {
    uint8_t col = j / 8;
    uint8_t row = j % 8;
    uint8_t fast, slow;
    if (col % 2) {
      fast = WIDTH / 2 - col / 2 - 1;
      slow = HEIGHT - row - 1;
    } else {
      fast = WIDTH / 2 + col / 2;
      slow = row;
    }
    if (slow > HEIGHT)
      continue;
    if (fast > WIDTH)
      continue;
    out[j] = in[fast + slow * WIDTH];
  }
}

int write_picture_to_register(uint8_t *buffer) {
  uint8_t scratch[BUFFER_SIZE + 1];

  scratch[0] = 0x24;

  map_buffer_to_pixels(buffer, &scratch[1]);

  i2c_write_blocking(I2C_PORT, ADDRESS, scratch, BUFFER_SIZE + 1, false);

  return 0;
}

int main() {
  uint8_t buffer[BUFFER_SIZE] = {0};

  stdio_init_all();

  i2c_init(I2C_PORT, FREQ);

  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);

  // initialise device - point to function register
  write_byte_to_register(COMMAND, 0xb);

  write_byte_to_register(FRAME_MODE, 0x0);
  write_byte_to_register(FRAME_NO, 0x0);
  write_byte_to_register(NULL0, 0x0);
  write_byte_to_register(NULL1, 0x0);
  write_byte_to_register(NULL2, 0x0);
  write_byte_to_register(SHUTDOWN, 0x1);

  // switch to frame 0
  write_byte_to_register(COMMAND, 0x0);

  // enable correct LED addresses (17 x lower 7, 1 x none)
  for (uint8_t j = 0; j < WIDTH; j++) {
    buffer[j] = 0x7f;
  }
  buffer[17] = 0x0;
  write_bytes_to_register(FRAME_MODE, buffer, 18);
  write_byte_to_register(COMMAND, 0x0);

  while (true) {
    for (uint8_t n = 0; n < WIDTH * HEIGHT; n++) {
      for (int j = 0; j < WIDTH * HEIGHT; j++) {
        buffer[j] = 0x0;
      }
      buffer[n] = 0xf;

      write_picture_to_register(buffer);
    }
  }
  return 0;
}
