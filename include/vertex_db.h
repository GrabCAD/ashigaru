#pragma once

#include <map>
#include <GL/glew.h>

/* the idea of VertexDB is that it enables vertex selection without regard to 
 * which vertex properties are available. This way, we gain the following:
 * 
 * 1. Communication with RenderAction is simplified, because the signature
 *    of RenderAction::Render need not contain all possible buffers used
 *    by the application.
 * 2. TiledView can do vertex selections without knowing what each vertex 
 *    carries. In the future, it will enable RenderAction to produce the 
 *    full image data from models, while ?TiledView will only do the 
 *    filtering. Alternatively, TiledView will construct a DB with pos and ID, 
 *    then RenderAction will load it with whatever else based on that data.
 * 3. We can add to this class vertex indexing/selection such that we can 
 *    e,g. select on priority without caring what else is in the VertexDB.
 * 
 * for now, though, it stores a variable number of equal-length named columns.
 */

class VertexDB {
    std::map<std::string, GLuint> m_buffers;
    unsigned int m_num_verts;
    
public:
    VertexDB() : m_num_verts{0} {}
    VertexDB(unsigned int num_verts) : m_num_verts{num_verts} {}
    VertexDB(unsigned int num_verts, std::map<std::string, GLuint> buffs) 
		: m_num_verts(num_verts) 
	{
		m_buffers = buffs;
	}
    
    void SetNumVerts(unsigned int num_verts) {
        if (m_buffers.size() != 0)
            throw std::runtime_error("Attempt to resize GL buffers.");
        m_num_verts = num_verts;
    }
    void AddBuffers(const std::map<std::string, GLuint>& buffs) { m_buffers.insert(buffs.begin(), buffs.end()); }
    void AddBuffer(const std::string& name, GLuint buff) { m_buffers[name] = buff; }
    
    GLuint GetBuffer(const std::string& name) const { return m_buffers.at(name); }
    unsigned int VertexCount() { return m_num_verts; }
};
