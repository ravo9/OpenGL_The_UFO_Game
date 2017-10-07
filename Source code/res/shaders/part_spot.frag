// Spot light data
#ifndef SPOT_LIGHT
#define SPOT_LIGHT
struct spot_light
{
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
struct material
{
	vec4 emissive;
	vec4 diffuse_reflection;
	vec4 specular_reflection;
	float shininess;
};
#endif

// Spot light calculation
vec4 calculate_spot(in spot_light spot, in material mat, in vec3 vertex_position, in vec3 transformed_normal, in vec3 view_dir, in vec4 tex_colour)
{
	// Calculate direction to the light
    vec3 L = normalize(spot.position - vertex_position);
    vec3 R = spot.direction;
    float final_dir_factor = dot((-1.0)*R, L);

    // Calculate distance to light
    float d = distance(spot.position, vertex_position);

    // Calculate attenuation value
    float attenuation = 1.0/(spot.constant + spot.linear * d + spot.quadratic * d * d);

	// Calculate spot light intensity :  (max( dot(light_dir, -direction), 0))^power
	float intens = pow(max(final_dir_factor, 0.0), spot.power);

	// Calculate light colour:  (intensity / attenuation) * light_colour
	 vec4 final_light_col = (intens * attenuation) * spot.light_colour;

	// Now use standard phong shading but using calculated light colour and direction
	vec4 diffuse = (mat.diffuse_reflection * final_light_col) * max(dot(transformed_normal, L), 0.0);
	vec3 half_vector = normalize(L + view_dir);
	vec4 specular = (mat.specular_reflection * final_light_col) * pow(max(dot(transformed_normal, half_vector), 0.0), mat.shininess);
	
	vec4 colour = ((mat.emissive + diffuse) * tex_colour) + specular;
	colour.a = 1.0;

	return colour;
}