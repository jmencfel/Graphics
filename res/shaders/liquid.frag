#version 440
// Directional light structure
#ifndef DIRECTIONAL_LIGHT
#define DIRECTIONAL_LIGHT
struct directional_light {
  vec4 ambient_intensity;
  vec4 light_colour;
  vec3 light_dir;
};
#endif
// A material structure
#ifndef MATERIAL
#define MATERIAL
struct material {
  vec4 emissive;
  vec4 diffuse_reflection;
  vec4 specular_reflection;
  float shininess;
};
#endif

// Forward declarations of used functions
vec4 calculate_direction(in directional_light light, in material mat, in vec3 normal, in vec3 view_dir,
                         in vec4 tex_colour);
vec3 calc_normal(in vec3 normal, in vec3 tangent, in vec3 binormal, in sampler2D normal_map, in vec2 tex_coord);


// Directional light information
uniform directional_light light;
// Material of the object being rendered
uniform material mat;
// Position of the eye
uniform vec3 eye_pos;
// Texture to sample from
uniform sampler2D tex;
uniform sampler2D tex2;

// Normal map to sample from
uniform sampler2D normal_map;
//time for waves

// Incoming position
layout(location = 0) in vec3 position;
// Incoming normal
layout(location = 1) in vec3 normal;
// Incoming texture coordinate
layout(location = 2) in vec2 tex_coord;
// Incoming tangent
layout(location = 3) in vec3 tangent;
// Incoming binormal
layout(location = 4) in vec3 binormal;
layout(location = 5) in float time;
// Outgoing colour
layout(location = 0) out vec4 colour;

void main() {
if((pow(tex_coord.x-0.5, 2)+pow(tex_coord.y-0.5,2))>=0.25)
discard;
else{
	vec2 new_tex_coord = vec2(tex_coord.x, 1-tex_coord.y);
	vec2 waves = vec2(0.025,0);
  vec3 view_dir = normalize(eye_pos - position);
  // Sample texture
  vec4 gold_tex = texture(tex2, tex_coord);
  vec4 tex_colour = texture(tex, new_tex_coord) ;
  vec4 mixed = mix(tex_colour, gold_tex, 0.5*dot(view_dir, normal));
  // Calculate directional light colour
   vec3 newnormal = calc_normal(normal, tangent, binormal, normal_map, tex_coord + waves * time);
  vec4 directional_colour =  calculate_direction(light, mat, normal, view_dir, mixed)+ calculate_direction(light, mat, newnormal, view_dir, mixed);
 
	colour = directional_colour;
	colour.a = 1.0- 0.5*dot(view_dir, normal);

}

  // *********************************
}