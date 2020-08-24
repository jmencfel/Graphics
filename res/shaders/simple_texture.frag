#version 440

// Sampler used to get texture colour
uniform sampler2D tex;
uniform sampler2D mask;
uniform sampler2D noise;
uniform sampler2D camera;

uniform float time;

uniform bool show_masks;
uniform bool help;
uniform bool fisheye;
uniform bool grayscale;
uniform bool LSD;
uniform bool chroma_ab;
uniform bool tv_noise;

// Incoming texture coordinate
layout (location = 0) in vec2 tex_coord;

const float PI = 3.1415926535;
const vec3 intensity = vec3(0.299, 0.587, 0.184);

vec3 rgb2hsv(vec3 c)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}
vec4 add_fisheye(sampler2D tex, vec2 tex_coord)
{
  float aperture = 170.0;
  float apertureHalf = 0.5 * aperture * (PI / 180.0);
  float maxFactor = sin(apertureHalf);
  vec2 uv;
  vec2 xy = 2.0 * tex_coord.xy - 1.0;
  float d = length(xy);
  if (d < (2.0-maxFactor))
  {
    d = length(xy * maxFactor);
    float z = sqrt(1.0 - d * d);
    float r = atan(d, z) / PI;
    float phi = atan(xy.y, xy.x); 
    uv.x = r * cos(phi) + 0.5;
    uv.y = r * sin(phi) + 0.5;
  }
  else
  {
  uv.xy = tex_coord.xy;
  }
return texture(tex,uv);
}
vec4 add_LSD(vec4 frame)
{
 	vec3 color = vec3(0.0, 0.0, 0.0);
	color = rgb2hsv(color);
	color.x = mod(((frame.x + frame.y + frame.z) * 0.7) + (time*0.5), 1.0);
	color.y = 1.0;
	color.z = 1.0;
	color = hsv2rgb(color);
	return mix(vec4(color, 1.0), frame, 0.0);
}
vec4 add_greyscale(vec4 frame)
{
  vec4 gray  = frame * vec4(intensity, 1.0);
  float grey = gray.r + gray.g + gray.b;
  return vec4(grey, grey, grey, 1.0);
}
vec4 add_chroma(vec4 frame)
{
	vec4 rValue = texture(tex, tex_coord - vec2(0.01*abs(sin(10.0*time)),0.01*abs(cos(10.0*time))));  
    vec4 gValue = texture(tex, tex_coord - vec2(-0.01*abs(cos(10.0*time)),0.01*abs(sin(10.0*time))));
    vec4 bValue = texture(tex, tex_coord - vec2(0.01*abs(sin(10.0*time)),-0.01*abs(cos(10.0*time))));  
	vec4 chroma = vec4(rValue.r,gValue.g,bValue.b, 1.0);
	return mix(frame, chroma, 0.8);
}
vec3 mod289(vec3 x) 
{
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}
vec2 mod289(vec2 x)
{
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec3 permute(vec3 x)
{
  return mod289(((x*34.0)+1.0)*x);
}
float snoise(vec2 v)
{
  const vec4 C = vec4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0
                      0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)
                     -0.577350269189626,  // -1.0 + 2.0 * C.x
                      0.024390243902439); // 1.0 / 41.0
// First corner
  vec2 i  = floor(v + dot(v, C.yy) );
  vec2 x0 = v -   i + dot(i, C.xx);

// Other corners
  vec2 i1;
  //i1.x = step( x0.y, x0.x ); // x0.x > x0.y ? 1.0 : 0.0
  //i1.y = 1.0 - i1.x;
  i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
  // x0 = x0 - 0.0 + 0.0 * C.xx ;
  // x1 = x0 - i1 + 1.0 * C.xx ;
  // x2 = x0 - 1.0 + 2.0 * C.xx ;
  vec4 x12 = x0.xyxy + C.xxzz;
  x12.xy -= i1;
  i = mod289(i); 
  vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))+ i.x + vec3(0.0, i1.x, 1.0 ));
  vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
  m = m*m ;
  m = m*m ;
  vec3 x = 2.0 * fract(p * C.www) - 1.0;
  vec3 h = abs(x) - 0.5;
  vec3 ox = floor(x + 0.5);
  vec3 a0 = x - ox;
  m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );
  vec3 g;
  g.x  = a0.x  * x0.x  + h.x  * x0.y;
  g.yz = a0.yz * x12.xz + h.yz * x12.yw;
  return 130.0 * dot(m, g);
}
float rand(vec2 co)
{
   return fract(sin(dot(co.xy,vec2(12.9898,78.233))) * 43758.5453);
}
vec4 add_tv_noise(vec4 frame)
{
 float noise = max(0.0, snoise(vec2(time, tex_coord.y * 0.3)) - 0.3) * (1.0 / 0.7);
    noise = noise + (snoise(vec2(time*10.0, tex_coord.y * 2.4)) - 0.5) * 0.15;
    float xpos = tex_coord.x - noise * noise * 0.25;
	vec4 final = texture(tex, vec2(xpos, tex_coord.y));
    final.rgb = mix(final.rgb, vec3(rand(vec2(tex_coord.y * time))), noise * 0.3).rgb;
    
    if (floor(mod(final.y * 0.25, 2.0)) == 0.0)
    {
        final.rgb *= 1.0 - (0.15 * noise);
    }
	return mix(frame, final, 0.5);
}

// Outgoing colour
layout (location = 0) out vec4 out_colour;


void main() {
 vec4 frame = texture(tex,tex_coord);


if(fisheye) frame = add_fisheye(tex, tex_coord);
if(LSD) frame = add_LSD(frame);
if (grayscale) frame = add_greyscale(frame);
if(chroma_ab) frame = add_chroma(frame);
if(tv_noise) frame = add_tv_noise(frame);

if(show_masks){
vec4 mask_col;
if(!help)
	{
	mask_col = texture(camera, tex_coord);
		if(sin(10.0*time)>=0 && (pow(tex_coord.x-0.83, 2.0)+pow(tex_coord.y-0.922, 2.0)/2.2f <= 0.0004))
			out_colour=vec4(1,0,0,1);
		else if(mask_col.x<=0.2)
			out_colour=frame;
		else
			out_colour=mask_col;
	}
	else
	{
	mask_col = texture(mask, tex_coord);
	if(mask_col != vec4(0,0,0,1))
		  out_colour = frame;
	 else
		  {
 		  out_colour = vec4(1.0, 1.0, 1.0, 1.0) - frame;
		   }
      }
	}else
	out_colour = frame;

	out_colour.a = 1.0;
  // *********************************
 
  
  
}