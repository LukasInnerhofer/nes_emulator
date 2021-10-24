#include "screen_libgraphics.h"
#include <libgraphics/texture.h>

namespace NesEmulator
{

ScreenLibGraphics::ScreenLibGraphics(std::shared_ptr<LibGraphics::Window> window) :
    m_texture{
        new LibGraphics::Texture(
            std::vector<uint8_t>(340 * 260 * 3), LibGraphics::Texture::SizeVector{340, 260})},
    m_rectangle{
        LibGraphics::Vector<float>{-1.0f,-1.0f},
        LibGraphics::Vector<float>{2.0f,2.0f},
        LibGraphics::Color{0, 0, 0},
        m_texture},
    m_window{window}
{
    
}

void ScreenLibGraphics::draw(LibNes::Screen::Pixel const &pixel)
{
    m_texture->setPixel(
        LibGraphics::Texture::PositionVector{pixel.position.x, pixel.position.y}, 
        LibGraphics::Color{pixel.color.r, pixel.color.g, pixel.color.b});
}

}