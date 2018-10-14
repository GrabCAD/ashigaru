#pragma once

#include <array>

namespace Ashigaru 
{
    template <typename DT>
    using Point = std::array<DT, 2>;
    
    template <typename DT>
    class Rect {
    public:
        using Corner = Point<DT>;
        
    private:
        Corner m_topleft, m_bottomright;
        
    public:
        Rect(DT top, DT left, DT bottom, DT right) 
            : m_topleft{Corner{top, left}}, m_bottomright{Corner{bottom, right}} {}
        
        Rect(Corner topleft, Corner bottomright)
            : m_topleft{topleft}, m_bottomright{bottomright} {}
        
        Rect() : Rect({}, {}, {}, {}) {}
                
        Corner getTopLeft() const { return m_topleft; }
        Corner getBottomRight() const { return m_bottomright; }
        
        DT top() const { return m_topleft[0]; }
        DT left() const { return m_topleft[1]; }
        DT bottom() const { return m_bottomright[0]; }
        DT right() const { return m_bottomright[1]; }
        
        DT Width() const { return m_bottomright[1] - m_topleft[1]; }
        DT Height() const { return m_topleft[0] - m_bottomright[0]; }
    };
}
