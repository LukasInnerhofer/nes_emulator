#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <vector>
#include <memory>

class Mapper;

struct Cartridge
{
	struct Rom
	{
		std::vector<uint8_t> m_trainer;
		std::vector<uint8_t> m_prgRom;
		std::vector<uint8_t> m_chrRom;

		static constexpr size_t headerSize = 4;
		static constexpr char header[headerSize + 1] = { 'N', 'E', 'S', 0x1A, 0 };
		static constexpr size_t trainerSize = 512;
		static constexpr size_t prgRomSizeMultiplier = 16384;
		static constexpr size_t chrRomSizeMultiplier = 8192;
	};

	std::shared_ptr<Rom> m_rom;
	std::shared_ptr<Mapper> m_mapper;
};

#endif // CARTRIDGE_H