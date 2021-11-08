#include "phat-beat.hpp"

#define CLK 24
#define DAT 23

int brightness = 255;
uint8_t b_l[8] = {0};
uint8_t b_r[9] = {0};

int32_t peak_l = 0;
int32_t peak_r = 0;

namespace pivumeter {
    void PHATBeat::update(uint32_t l, uint32_t r) {
        int32_t meter_l = (l * 8 * brightness) / 16384;
        int32_t meter_r = (r * 8 * brightness) / 16384;

        for(auto led = 0u; led < 8u; led++) {
            b_l[led] = std::min(meter_l, brightness);
            meter_l -= brightness;
            meter_l = std::max(0, meter_l);

            b_r[led] = std::min(meter_r, brightness);
            meter_r -= brightness;
            meter_r = std::max(0, meter_r);
        }

        /*printf("L:");
        for(auto led = 0u; led < 8u; led++) {
            printf("%02x", b_l[led]);
        }
        printf("\n");

        printf("R:");
        for(auto led = 0u; led < 8u; led++) {
            printf("%02x", b_r[led]);
        }
        printf("\n");*/
    }

    void PHATBeat::sof() {
        for(auto x = 0u; x < 4; x++) {
            write_byte(0);
        }
    }

    void PHATBeat::eof() {
        for(auto x = 0u; x < 5; x++) {
            write_byte(0);
        }
    }

    void PHATBeat::write_byte(uint8_t byte) {
        for(auto x = 0u; x < 8; x++) {
            gpiod_line_set_value(line_dat, byte & 0b10000000);
            gpiod_line_set_value(line_clk, 1);
            byte <<= 1;
            gpiod_line_set_value(line_clk, 0);
        }
    }

    void PHATBeat::render() {
        sof();
        for(auto led = 0u; led < 8u; led++) {
            uint8_t led_r = 7 - led;
            unsigned int blue = 255 - (led_r * 32);
            blue = (blue * b_r[led_r]) / 255;
            unsigned int red = led_r * 8;
            red = (red * b_r[led_r]) / 255;
            write_byte(0b11100000 | 15);
            write_byte(blue);
            write_byte(0);
            write_byte(red);
        }
        for(auto led = 0u; led < 8u; led++) {
            uint8_t led_l = led;
            unsigned int blue = 255 - (led_l * 32);
            blue = (blue * b_l[led_l]) / 255;
            unsigned int red = led_l * 8;
            red = (red * b_l[led_l]) / 255;
            write_byte(0b11100000 | 15);
            write_byte(blue);
            write_byte(0);
            write_byte(red);
        }
    }

    int PHATBeat::init() {
        chip = gpiod_chip_open("/dev/gpiochip0");

        line_clk = gpiod_chip_get_line(chip, CLK);
        if(gpiod_line_request_output(line_clk, consumer, 0)) {
            std::cout << "Failed to set up Clock pin" << std::endl;
            return 1;
        }

        line_dat = gpiod_chip_get_line(chip, DAT);
        if(gpiod_line_request_output(line_dat, consumer, 0)) {
            std::cout << "Failed to set up Data pin" << std::endl;
            return 1;
        }

        return 0;
    }

    void PHATBeat::deinit() {
        gpiod_line_release(line_clk);
        gpiod_line_release(line_dat);
        gpiod_chip_close(chip);
    }
}
