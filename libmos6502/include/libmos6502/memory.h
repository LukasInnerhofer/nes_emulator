#pragma once

#include <cstdint>

namespace LibMos6502
{

class Memory
{
	public:
		virtual uint8_t read(uint16_t addr) = 0;
		virtual void write(uint16_t addr, uint8_t data) = 0;
};

}