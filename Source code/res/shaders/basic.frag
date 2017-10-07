#version 450 core

// Directional light data
#ifndef DIRECTIONAL_LIGHT
#define DIRECTIONAL_LIGHT
struct directional_light {
  vec4 ambient_intensity;
  vec4 light_colour;
  vec3 light_dir;
};
#endif

// Spot light data
#ifndef SPOT_LIGHT
#define SPOT_LIGHT
struct spot_light {
  vec4 light_colour;
  vec3 position;
  vec3 direction;
  float constant;
  float linear;
  float quadratic;
  float power;
};
#endif

// Material data
#ifndef MATERIAL
#define MATERIAL
struct material {
  vec4 emissive;
  vec4 diffuse_reflection;
  vec4 specular_reflection;
  float shininess;
};
#endif

// Function, which calculates the spot light
vec4 calculate_spot(in spot_light spot, in material mat, in vec3 position, in vec3 normal, in vec3 view_dir, in vec4 tex_colour);

// Function, which calculates the shadow
float calculate_shadow(in sampler2D shadow_map, in vec4 light_space_pos);

// Function, which calculates the directional light
vec4 calculate_direction(in directional_light light, in material mat, in vec3 normal, in vec3 view_dir, in vec4 tex_colour);

// Function, which calculates normals for normal mapping
vec3 calc_normal(in vec3 normal, in vec3 tangent, in vec3 binormal, in sampler2D normal_map, in vec2 tex_coord);

uniform spot_light spot;
uniform directional_light light;
uniform material mat;
uniform vec3 eye_pos;
uniform float ilumination;
uniform sampler2D tex;
uniform sampler2D normal_map;
uniform sampler2D shadow_map;

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex_coord;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 tangent_out;
layout(location = 4) in vec3 binormal_out;
layout(location = 6) in vec4 light_vertex;

layout(location = 0) out vec4 out_colour;


void main() {

 float shade = calculate_shadow(shadow_map, light_vertex);

 vec4 tex_colour = texture(tex, tex_coord);
 vec3 view_dir = normalize(eye_pos - position);
 vec3 normal2 = calc_normal(normal, tangent_out, binormal_out, normal_map, tex_coord);

 vec4 colourB = calculate_spot(spot, mat, position, normal2, view_dir, tex_colour);
 vec4 colourA = calculate_direction(light, mat, normal2, view_dir, tex_colour);

 // Calculate a final colour (made of spot light, directional light and shadowing)
 vec4 colA = shade * colourB + colourA * shade;

 out_colour = colA * ilumination;
 out_colour.a = 1.0;

}
