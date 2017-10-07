#version 450 core

uniform mat4 MVP;
uniform mat4 M;
uniform mat3 N;
uniform mat4 lightMVP;

layout (location = 0) in vec3 position;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 binormal;
layout(location = 4) in vec3 tangent;
layout (location = 10) in vec2 tex_coord_in;

layout(location = 0) out vec3 vertex_position;
layout(location = 1) out vec2 tex_coord;
layout(location = 2) out vec3 transformed_normal;
layout(location = 3) out vec3 tangent_out;
layout(location = 4) out vec3 binormal_out;
layout(location = 6) out vec4 light_vertex;


void main()
{
	gl_Position = MVP * vec4(position, 1.0);

	tex_coord = tex_coord_in;

    vertex_position = (M * vec4(position, 1.0)).xyz;

    transformed_normal = N * normal;

    tangent_out = N * tangent;

    binormal_out = N * binormal;

	light_vertex = lightMVP * vec4(position, 1.0);
}