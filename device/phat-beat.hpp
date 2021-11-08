#include "device.hpp"

#include <iostream>
#include "gpiod.h"

namespace pivumeter {
    class PHATBeat final : public Device {
        public:
            struct gpiod_chip *chip;
            struct gpiod_line *line_clk;
            struct gpiod_line *line_dat;
            const char *consumer = "PiVUMeter: PHAT Beat";
            void update(uint32_t l, uint32_t r) override;
            void render() override;
            int init() override;
            void deinit() override;

            void sof();
            void eof();
            void write_byte(uint8_t byte);
    };
};
