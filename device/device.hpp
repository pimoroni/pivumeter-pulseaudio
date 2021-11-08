#pragma once

#include <cstdint>

namespace pivumeter {
    class Device {
	public:
            virtual void update(uint32_t l, uint32_t r) {}
            virtual void render() {};
	    virtual int init() {return 0;};
	    virtual void deinit() {};
    };
};
