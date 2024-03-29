#include <iomanip>
#include <stdexcept>
#include <string>
#include <sstream>

#include "libmos6502/mos6502.h"

namespace LibMos6502
{

Mos6502::Mos6502(NonNullSharedPtr<Memory> memory) :
	m_memory{memory},
	m_pc{pcDefault}, 
	m_sp{spDefault}, 
	m_acc{accDefault}, 
	m_x{xDefault}, 
	m_y{yDefault}, 
	m_status{statusDefault},
	m_cycles{0}, 
	m_newPc{0},
	m_addrMode{AddressMode::Abs}
{

}

void Mos6502::reset()
{
	m_sp = spDefault;
	m_acc = accDefault;
	m_x = xDefault;
	m_y = yDefault;
	m_status = statusDefault;
	m_pc = read16(resetVector);
}

void Mos6502::step(
#if defined(LIBMOS6502_LOG)
	std::ofstream& log
#endif
)
{
	m_cycles = 0;
	const uint8_t opCode{read8(m_pc)};
	m_newPc = m_pc + 1;

	const Instruction instruction{m_instructions[opCode]};

#if defined(LIBMOS6502_LOG)
	log << std::hex << std::setfill('0') << std::setw(4) << std::right << std::uppercase << 
		m_pc << " " << std::setw(2) << static_cast<int>(opCode) << " " <<
		instruction.m_name << " " <<
		"A:" << std::setw(2) << static_cast<int>(m_acc) << " " << 
		"X:" << std::setw(2) << static_cast<int>(m_x) << " " << 
		"Y:" << std::setw(2) << static_cast<int>(m_y) << " " << 
		"P:" << std::setw(2) << (m_status.to_ulong() & 0xEF) << " " << 
		"SP:" << std::setw(2) << static_cast<int>(m_sp);
#endif

	m_addrMode = instruction.m_addressMode;
	(this->*instruction.m_instruction)();

	m_cycles += m_cycles == 1;

	m_pc = m_newPc;
}

uint8_t Mos6502::getCycles()
{
	return m_cycles;
}

uint8_t Mos6502::read8(uint16_t addr)
{
	++m_cycles;
	return m_memory->read(addr);
}

uint16_t Mos6502::read16(uint16_t addr)
{
	return (read8(addr + 1) << 8) | read8(addr);
}

uint16_t Mos6502::readPage16(uint16_t addr)
{
	return (read8((addr + 1) & 0xFF) << 8) | read8(addr);
}

uint8_t Mos6502::readArg8(uint16_t addr)
{
	++m_newPc;
	return read8(addr);
}

uint16_t Mos6502::readArg16(uint16_t addr)
{
	m_newPc += 2;
	return read16(addr);
}

void Mos6502::write8(uint16_t addr, uint8_t data)
{
	++m_cycles;
	m_memory->write(addr, data);
}

void Mos6502::push8(uint8_t data)
{
	write8(stackOffset + m_sp--, data);
}

void Mos6502::push16(uint16_t data)
{
	push8(data >> 8);
	push8(data & 0xFF);
}

uint8_t Mos6502::pull8()
{
	return read8(stackOffset + ++m_sp);
}

uint16_t Mos6502::pull16()
{
	uint16_t data{pull8()};
	data |= pull8() << 8;
	return data;
}

void Mos6502::pullStatus()
{
	m_status = (pull8() & 0xCF) | (m_status.to_ulong() & 0x30);
}

uint16_t Mos6502::readAddress(bool assumePageCross = false)
{
	uint16_t addr{0};

	switch (m_addrMode)
	{
	case AddressMode::Abs:
		addr = readArg16(m_pc + 1);
		break;
	case AddressMode::Rel:
	case AddressMode::Imm:
		addr = m_pc + 1;
		++m_newPc;
		break;

	case AddressMode::ZoP:
		addr = readArg8(m_pc + 1);
		break;

	case AddressMode::ZpX:
		++m_cycles;
		addr = (readArg8(m_pc + 1) + m_x) & 0xFF;
		break;

	case AddressMode::ZpY:
		++m_cycles;
		addr = (readArg8(m_pc + 1) + m_y) & 0xFF;
		break;

	case AddressMode::AbX:
		addr = readArg16(m_pc + 1);
		if ((((addr & 0xFF) + m_x) & 0xFF00) != 0 || assumePageCross)
		{
			++m_cycles;
		}
		addr += m_x;
		break;

	case AddressMode::AbY:
		addr = readArg16(m_pc + 1);
		if ((((addr & 0xFF) + m_y) & 0xFF00) != 0 || assumePageCross)
		{
			++m_cycles;
		}
		addr += m_y;
		break;

	case AddressMode::Pre:
		++m_cycles; // due to post increment
		addr = (readArg8(m_pc + 1) + m_x) & 0xFF;
		addr = readPage16(addr);
		break;

	case AddressMode::Pos:
		addr = readArg8(m_pc + 1);
		addr = readPage16(addr);
		if ((((addr & 0xFF) + m_y) & 0xFF00) != 0 || assumePageCross)
		{
			++m_cycles;
		}
		addr += m_y;
		break;

	case AddressMode::Ind:
		addr = readArg16(m_pc + 1);
		// 6502 fetches incorrectly if address is at page boundary
		if ((addr & 0xFF) == 0xFF)
		{
			addr = (read8(addr & 0xFF00) << 8) | read8(addr);
		}
		else
		{
			addr = read16(addr);
		}
		break;

	default:
		addr = 0;
		break;
	}

	return addr;
}

void Mos6502::setNZ(uint8_t src)
{
	m_status[StatusBits::Negative] = src & 0x80;
	m_status[StatusBits::Zero] = src == 0;
}

void Mos6502::ADC()
{
	const uint8_t accOld{m_acc};
	const uint8_t src{read8(readAddress())};
	const auto sum{static_cast<uint16_t>(m_acc + src + m_status[StatusBits::Carry])};
	m_acc = sum & 0xFF;

	m_status[StatusBits::Carry] = sum >= 0x100;
	setNZ(m_acc);
	m_status[StatusBits::Overflow] =		// Overflow occured if:
		(!((accOld ^ src) & 0x80) &&		// Both numbers had the same sign before AND
			((accOld ^ sum) & 0x80));	// result has a different sign
}

void Mos6502::SBC() 
{
	const uint8_t accOld{m_acc};
	const uint8_t src{read8(readAddress())};
	const uint16_t dif{static_cast<uint16_t>(m_acc - src - ~m_status[StatusBits::Carry])};
	m_acc = dif & 0xFF;

	m_status[StatusBits::Carry] = dif < 0x100;
	setNZ(m_acc);
	m_status[StatusBits::Overflow] =		// Overflow occured if:
		(((accOld ^ src) & 0x80) &&		// The numbers had a different sign before AND
			((accOld ^ dif) & 0x80));	// result has a different sign than the minuend
}

void Mos6502::AND()
{
	m_acc &= read8(readAddress());
	setNZ(m_acc);
}

void Mos6502::ASL()
{
	if (m_addrMode == AddressMode::Acc)
	{
		m_status[StatusBits::Carry] = m_acc & 0x80;
		m_acc <<= 1;
		setNZ(m_acc);
	}
	else
	{
		const uint16_t addr{readAddress(true)};
		uint8_t src{read8(addr)};
		m_status[StatusBits::Carry] = src & 0x80;
		++m_cycles; // rmw instructions take one extra cycle during modify
		write8(addr, src <<= 1);
		setNZ(src);
	}
}

void Mos6502::BIT()
{
	const uint8_t src{read8(readAddress())};
	m_status[StatusBits::Negative] = src & 0x80;
	m_status[StatusBits::Overflow] = src & 0x40;
	m_status[StatusBits::Zero] = (src & m_acc) == 0;
}

void Mos6502::BRK()
{
	push16(m_pc + 2);
	push8(static_cast<uint8_t>(m_status.to_ulong()));
	m_status[StatusBits::Interrupt] = true;
	m_pc = read16(irqVector);
}

void Mos6502::compare(uint8_t reg)
{
	const uint8_t src{read8(readAddress())};
	m_status[StatusBits::Carry] = reg >= src;
	setNZ(reg - src);
}

void Mos6502::CMP()
{
	compare(m_acc);
}

void Mos6502::CPX()
{
	compare(m_x);
}

void Mos6502::CPY()
{
	compare(m_y);
}

uint8_t Mos6502::decrement(uint8_t src)
{
	++m_cycles; // rmw instructions take one extra cycle during modify
	setNZ(--src);
	return src;
}

uint8_t Mos6502::increment(uint8_t src)
{
	++m_cycles; // rmw instructions take one extra cycle during modify
	setNZ(++src);
	return src;
}

void Mos6502::DEC()
{
	const uint16_t addr{readAddress(true)};
	write8(addr, decrement(read8(addr)));
}

void Mos6502::DEX()
{
	m_x = decrement(m_x);
}

void Mos6502::DEY()
{
	m_y = decrement(m_y);
}

void Mos6502::INC()
{
	const uint16_t addr{readAddress(true)};
	write8(addr, increment(read8(addr)));
}

void Mos6502::INX()
{
	m_x = increment(m_x);
}

void Mos6502::INY()
{
	m_y = increment(m_y);
}

void Mos6502::EOR()
{
	setNZ(m_acc ^= read8(readAddress()));
}

void Mos6502::JMP()
{
	m_newPc = readAddress();
}

void Mos6502::JSR()
{
	++m_cycles; // Due to stack push
	push16(m_pc + 2);
	m_newPc = readAddress();
}

void Mos6502::LSR()
{
	if (m_addrMode == AddressMode::Acc)
	{
		m_status[StatusBits::Carry] = m_acc & 0x01;
		m_acc >>= 1;
		m_status[StatusBits::Zero] = m_acc == 0;
	}
	else
	{
		const uint16_t addr{readAddress(true)};
		uint8_t src{read8(addr)};
		m_status[StatusBits::Carry] = src & 0x01;
		++m_cycles; // rmw instructions take one extra cycle during modify
		write8(addr, src >>= 1);
		m_status[StatusBits::Zero] = src == 0;
	}
	m_status[StatusBits::Negative] = 0;
}

void Mos6502::NOP()
{
	// TODO: Prove Riemann's Hypothesis
}

void Mos6502::ORA()
{
	m_acc |= read8(readAddress());
	setNZ(m_acc);
}

void Mos6502::ROL() 
{
	if (m_addrMode == AddressMode::Acc)
	{
		const bool oldCarry{m_status[StatusBits::Carry]};
		m_status[StatusBits::Carry] = m_acc & 0x80;
		m_acc = (m_acc << 1) | static_cast<uint8_t>(oldCarry);
		setNZ(m_acc);
	}
	else
	{
		const uint16_t addr{readAddress(true)};
		uint8_t src{read8(addr)};
		const bool oldCarry{m_status[StatusBits::Carry]};
		m_status[StatusBits::Carry] = src & 0x80;
		++m_cycles; // rmw instructions take one extra cycle during modify
		write8(addr, src = (src << 1) | static_cast<uint8_t>(oldCarry));
		setNZ(src);
	}
}

void Mos6502::ROR()
{
	if (m_addrMode == AddressMode::Acc)
	{
		const bool oldCarry{m_status[StatusBits::Carry]};
		m_status[StatusBits::Carry] = m_acc & 0x01;
		m_acc = (m_acc >> 1) | (static_cast<uint8_t>(oldCarry) << 7);
		setNZ(m_acc);
	}
	else
	{
		const uint16_t addr{readAddress(true)};
		uint8_t src{read8(addr)};
		const bool oldCarry{m_status[StatusBits::Carry]};
		m_status[StatusBits::Carry] = src & 0x01;
		++m_cycles; // rmw instructions take one extra cycle during modify
		write8(addr, src = (src >> 1) | (static_cast<uint8_t>(oldCarry) << 7));
		setNZ(src);
	}
}

void Mos6502::RTI()
{
	m_cycles += 2; // stack pull
	pullStatus();
	m_newPc = pull16();
}

void Mos6502::RTS()
{
	m_cycles += 2 + 1; // stack pull + post increment of the pc
	m_newPc = pull16() + 1;
}

void Mos6502::TAX()
{
	setNZ(m_x = m_acc);
}

void Mos6502::TAY()
{
	setNZ(m_y = m_acc);
}

void Mos6502::TSX()
{
	setNZ(m_x = m_sp);
}

void Mos6502::TXA()
{
	setNZ(m_acc = m_x);
}

void Mos6502::TXS()
{
	m_sp = m_x;
}

void Mos6502::TYA()
{
	setNZ(m_acc = m_y);
}

void Mos6502::STA()
{
	write8(readAddress(true), m_acc);
}

void Mos6502::STX()
{
	write8(readAddress(), m_x);
}

void Mos6502::STY()
{
	write8(readAddress(), m_y);
}

void Mos6502::SEC()
{
	m_status[StatusBits::Carry] = true;
}
void Mos6502::SED()
{
	m_status[StatusBits::Decimal] = true;
}
void Mos6502::SEI()
{
	m_status[StatusBits::Interrupt] = true;
}
void Mos6502::CLC() 
{
	m_status[StatusBits::Carry] = false;
}
void Mos6502::CLD() 
{
	m_status[StatusBits::Decimal] = false;
}
void Mos6502::CLI() 
{
	m_status[StatusBits::Interrupt] = false;
}
void Mos6502::CLV() 
{
	m_status[StatusBits::Overflow] = false;
}

void Mos6502::branch(bool condition)
{
	if (condition)
	{
		++m_cycles;
		const uint16_t oldPc{m_pc};
		m_newPc += read8(readAddress());
		m_cycles += ((oldPc + 2) & 0xFF00) != (m_newPc & 0xFF00);
	}
	else
	{
		++m_newPc;
	}
}
void Mos6502::BCC()
{
	branch(!m_status[StatusBits::Carry]);
}
void Mos6502::BCS()
{
	branch(m_status[StatusBits::Carry]);
}
void Mos6502::BEQ()
{
	branch(m_status[StatusBits::Zero]);
}
void Mos6502::BMI()
{
	branch(m_status[StatusBits::Negative]);
}
void Mos6502::BNE()
{
	branch(!m_status[StatusBits::Zero]);
}
void Mos6502::BPL()
{
	branch(!m_status[StatusBits::Negative]);
}
void Mos6502::BVC()
{
	branch(!m_status[StatusBits::Overflow]);
}
void Mos6502::BVS()
{
	branch(m_status[StatusBits::Overflow]);
}

void Mos6502::LDA()
{
	setNZ(m_acc = read8(readAddress()));
}
void Mos6502::LDX()
{
	setNZ(m_x = read8(readAddress()));
}
void Mos6502::LDY()
{
	setNZ(m_y = read8(readAddress()));
}

void Mos6502::PHA()
{
	++m_cycles; // stack push
	push8(m_acc);
}
void Mos6502::PHP()
{
	++m_cycles; // stack push
	push8(static_cast<uint8_t>(m_status.to_ulong()) | 0x30);
}
void Mos6502::PLA()
{
	m_cycles += 2; // stack pull
	setNZ(m_acc = pull8());
}
void Mos6502::PLP()
{
	m_cycles += 2; // stack pull
	pullStatus();
}

void Mos6502::ILL()
{
	// TODO: Call the police.
}

}
