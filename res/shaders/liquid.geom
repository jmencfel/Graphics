#version 440

// Scale for normal
uniform float time;

// Layout of incoming data
layout(triangles) in;
// Layout of outgoing data
layout(triangle_strip, max_vertices = 3) out;

// Outgoing position
layout (location = 0) in vec3 position[];
// Outgoing transformed normal
layout (location = 1) in vec3 normal[];
// Outgoing texture coordinate
layout (location = 2) in vec2 tex_coord[];
layout(location = 3) in vec3 binormal[];
// Incoming tangent
layout(location = 4) in vec3 tangent[];

// Outgoing position
layout (location = 0) out vec3 vertex_position;
// Outgoing transformed normal
layout (location = 1) out vec3 transformed_normal;
// Outgoing texture coordinate
layout (location = 2) out vec2 tex_coord_out;
layout(location = 3) out vec3 binormal_out;
// Incoming tangent
layout(location = 4) out vec3 tangent_out;

layout(location = 5) out float time_out;
void main() 
{

  // Calculate Face Normal
 
  for (int i = 0; i < 3; i++) 
 {
	vertex_position = position[i];
	transformed_normal = normal[i];
	tex_coord_out = tex_coord[i];
	binormal_out = binormal[i];
	tangent_out = tangent[i];
	time_out = time;
	float x = position[i].x;
	float z = position[i].z;
	gl_Position = gl_in[i].gl_Position+vec4(0.0, sin(time)*sin(x), 0.0, 0.0);
	EmitVertex();
  }
  EndPrimitive();
    // *********************************
  }
