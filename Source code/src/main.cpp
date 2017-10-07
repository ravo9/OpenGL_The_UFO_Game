#include <glm\glm.hpp>
#include <graphics_framework.h>

using namespace std;
using namespace std::chrono;
using namespace graphics_framework;
using namespace glm;

// Maps containing meshes with simple texturing and meshes with normal mapping.
map<string, mesh> meshes;
map<string, mesh> meshes_out;
map<string, mesh> explode_meshes;

mesh render_cube;
mesh skybox;
mesh terr;

geometry geom;
geometry screen_quad;
texture smoke_tex;
texture tex[4];

// Variables used for fire.
const unsigned int MAX_PARTICLES = 16000;
vec4 positionsSM[MAX_PARTICLES];
vec4 velocitysSM[MAX_PARTICLES];
GLuint vao;
GLuint G_Position_buffer, G_Velocity_buffer;

// Effects - effect for meshes with simple texturing, effect for meshes with normal mapping, effect for skybox and effect for shadowing.
effect eff;
effect smoke_eff;
effect smoke_compute_eff;
effect norm_eff;
effect sky_eff;
effect shadow_eff;
effect mblur;
effect tex_eff;
effect explode;
effect ter_eff;

// Frame buffers.
frame_buffer frames[2];
frame_buffer temp_frame;
frame_buffer tframe;

// Shadow map (for shadows), cubemap (for Skybox).
shadow_map shadow;
cubemap cube_map;

// Maps containing textures and normal maps.
map<string, texture> textures;
map<string, texture> textures_normals;

// Cameras and appropriate buttons to choose the camera.
free_camera cam; // 1
target_camera cam_t; // 2, 3
chase_camera cam_c; // 4
target_camera tcam;
target_camera tcam2;

// Variables used for option choosing and for the control.
int bomb = 0;
int shoot = 0;
int mblur_on = 0;
int stop_moving_ufo = 0;
int render_out = 0;
int chosen_camera = 1;
int control_on = 0;
int fireworks_start = 0;
int current_frame = 0;
float ilumination = 1.0;
float explode_f = 0.0f;
vec3 explode_pos;
vec3 prevpos = tcam2.get_position();
vec3 zmi5;

// Lights.
spot_light spot;
directional_light light;

// Random number engine.
default_random_engine e1;
uniform_int_distribution<int> dist(-10, 10);

// Variables used for mouse control.
double cursor_x = 0.0;
double cursor_y = 0.0;

// Variable used to create a movement of the UFO around the scene.
float t = 1;

// Mouse control initialising.
bool initialise()
{
	glfwSetInputMode(renderer::get_window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwGetCursorPos(renderer::get_window(), &cursor_x, &cursor_y);
	return true;
}

// Function which generates terrain.
void generate_terrain(geometry &geom, const texture &height_map, unsigned int width, unsigned int depth, float height_scale) 
{
	vector<vec3> positions;
	vector<vec3> normals;
	vector<vec2> tex_coords;
	vector<vec4> tex_weights;
	vector<unsigned int> indices;

	glBindTexture(GL_TEXTURE_2D, height_map.get_id());
	auto data = new vec4[height_map.get_width() * height_map.get_height()];
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, (void *)data);
	float width_point = static_cast<float>(width) / static_cast<float>(height_map.get_width());
	float depth_point = static_cast<float>(depth) / static_cast<float>(height_map.get_height());
	vec3 point;

	for (int x = 0; x < height_map.get_width(); ++x)
	{
		point.x = -(width / 2.0f) + (width_point * static_cast<float>(x));
		for (int z = 0; z < height_map.get_height(); ++z)
		{
			point.z = depth / 2 + depth_point*z;
			point.y = data[(z * height_map.get_width()) + x].y * height_scale;
			positions.push_back(point);
		}
	}

	for (unsigned int x = 0; x < height_map.get_width() - 1; ++x) 
	{
		for (unsigned int y = 0; y < height_map.get_height() - 1; ++y) 
		{
			unsigned int top_left = (y * height_map.get_width()) + x;
			unsigned int top_right = (y * height_map.get_width()) + x + 1;
			unsigned int bottom_left = ((y + 1) * height_map.get_width()) + x;
			unsigned int bottom_right = ((y + 1) * height_map.get_height()) + x + 1;
			indices.push_back(top_left);
			indices.push_back(bottom_right);
			indices.push_back(bottom_left);
			indices.push_back(top_left);
			indices.push_back(top_right);
			indices.push_back(bottom_right);
		}
	}

	normals.resize(positions.size());

	for (unsigned int i = 0; i < indices.size() / 3; ++i) 
	{
		auto idx1 = indices[i * 3];
		auto idx2 = indices[i * 3 + 1];
		auto idx3 = indices[i * 3 + 2];
		vec3 side1 = positions[idx1] - positions[idx3];
		vec3 side2 = positions[idx1] - positions[idx2];
		vec3 normal = normalize(cross(side2, side1));
		normals[idx1] = normals[idx1] + normal;
		normals[idx2] = normals[idx2] + normal;
		normals[idx3] = normals[idx3] + normal;
	}

	for (auto &n : normals) 
		n = normalize(n);	
	for (unsigned int x = 0; x < height_map.get_width(); ++x) { for (unsigned int z = 0; z < height_map.get_height(); ++z) {tex_coords.push_back(vec2(width_point * x, depth_point * z));}}
	for (unsigned int x = 0; x < height_map.get_width(); ++x) 
	{
		for (unsigned int z = 0; z < height_map.get_height(); ++z) 
		{
			vec4 tex_weight(clamp(1.0f - abs(data[(height_map.get_width() * z) + x].y - 0.0f) / 0.25f, 0.0f, 1.0f),
				clamp(1.0f - abs(data[(height_map.get_width() * z) + x].y - 0.15f) / 0.25f, 0.0f, 1.0f),
				clamp(1.0f - abs(data[(height_map.get_width() * z) + x].y - 0.5f) / 0.25f, 0.0f, 1.0f),
				clamp(1.0f - abs(data[(height_map.get_width() * z) + x].y - 0.9f) / 0.25f, 0.0f, 1.0f));
			auto total = tex_weight.x + tex_weight.y + tex_weight.z + tex_weight.w;
			tex_weight = tex_weight / total;
			tex_weights.push_back(tex_weight);
		}
	}

	geom.add_buffer(positions, BUFFER_INDEXES::POSITION_BUFFER);
	geom.add_buffer(normals, BUFFER_INDEXES::NORMAL_BUFFER);
	geom.add_buffer(tex_coords, BUFFER_INDEXES::TEXTURE_COORDS_0);
	geom.add_buffer(tex_weights, BUFFER_INDEXES::TEXTURE_COORDS_1);
	geom.add_index_buffer(indices);
	delete[] data;
}


bool load_content() 
{
  tframe = frame_buffer(renderer::get_screen_width(), renderer::get_screen_height());
  render_cube = mesh(geometry_builder::create_box());
  render_cube.get_transform().scale = vec3(5.0f, 5.0f, 5.0f);

  meshes_out["q1"] = mesh(geometry_builder::create_sphere(20, 20));
  meshes_out["q1"].get_transform().scale = vec3(10.0f, 10.0f, 10.0f);

  // Smoke
  default_random_engine rand(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
  uniform_real_distribution<float> dist;

  for (unsigned int i = 0; i < MAX_PARTICLES; ++i) 
  {
	  positionsSM[i] = vec4(((2.0f * dist(rand)) - 1.0f) / 10.0f, 5.0 * dist(rand), 0.0f, 0.0f);
	  velocitysSM[i] = vec4(0.0f, 0.1f + dist(rand), 0.0f, 0.0f);
  }

  smoke_tex = texture("textures/smoke.png");

  texture height_map("textures/heightmap.jpg");
  generate_terrain(geom, height_map, 20, 20, 2.0f);
  terr = mesh(geom);

  tex[0] = texture("textures/sand.jpg");
  tex[1] = texture("textures/grass.jpg");
  tex[2] = texture("textures/stone.jpg");
  tex[3] = texture("textures/snow.jpg");

  // Frame buffers
  frames[0] = frame_buffer(renderer::get_screen_width(), renderer::get_screen_height());
  frames[1] = frame_buffer(renderer::get_screen_width(), renderer::get_screen_height());
  temp_frame = frame_buffer(renderer::get_screen_width(), renderer::get_screen_height());

  // Screen quad
  vector<vec3> positions{ vec3(-1.0f, -1.0f, 0.0f), vec3(1.0f, -1.0f, 0.0f), vec3(-1.0f, 1.0f, 0.0f),
	  vec3(1.0f, 1.0f, 0.0f) };
  vector<vec2> tex_coords{ vec2(0.0, 0.0), vec2(1.0f, 0.0f), vec2(0.0f, 1.0f), vec2(1.0f, 1.0f) };
  screen_quad.add_buffer(positions, BUFFER_INDEXES::POSITION_BUFFER);
  screen_quad.add_buffer(tex_coords, BUFFER_INDEXES::TEXTURE_COORDS_0);
  screen_quad.set_type(GL_TRIANGLE_STRIP);

  shadow = shadow_map(renderer::get_screen_width(), renderer::get_screen_height());

  // Skybox.
  skybox = mesh(geometry_builder::create_box());
  skybox.get_transform().scale += vec3(1000.0f, 1000.0f, 1000.0f);

  // Creating and setting of the meshes with simple texturing.
  meshes["s1"] = mesh(geometry_builder::create_sphere(20, 20));
  meshes["s1"].get_transform().scale = vec3(10.0f, 10.0f, 10.0f);
  meshes["s1"].get_transform().translate(vec3(400.0f, 300.0f, 0.0f));

  meshes["s2"] = mesh(geometry_builder::create_sphere(20, 20));
  meshes["s2"].get_transform().scale = vec3(10.0f, 10.0f, 10.0f);
  meshes["s2"].get_transform().translate(vec3(400.0f, 350.0f, 0.0f));

  meshes["s3"] = mesh(geometry_builder::create_sphere(20, 20));
  meshes["s3"].get_transform().scale = vec3(10.0f, 10.0f, 10.0f);
  meshes["s3"].get_transform().translate(vec3(400.0f, 200.0f, 0.0f));

  // A bomb.
  meshes["s4"] = mesh(geometry_builder::create_sphere(10, 10));
  meshes["s4"].get_transform().position = meshes["ufo"].get_transform().position;

  meshes["moon"] = mesh(geometry_builder::create_sphere(20, 20));
  meshes["moon"].get_transform().scale = vec3(20.0f, 20.0f, 20.0f);
  meshes["moon"].get_transform().translate(vec3(400.0f, 270.0f, 0.0f));

  terr.get_transform().scale *= vec3(40.0f, 40.0f, 40.0f);
  terr.get_transform().translate(vec3(0.0f, -60.0f, -740.0f));

  // Creating and setting of the meshes with normal mapping.
  meshes["house"] = mesh(geometry("models/house.obj"));
  meshes["house"].get_transform().scale = vec3(0.04f, 0.04f, 0.04f);
  meshes["house"].get_transform().rotate(vec3(0, half_pi<float>(), 0));
  meshes["house"].get_transform().translate(vec3(-20.0f, 0.0f, -15.0f));
 
  meshes["windmill"] = mesh(geometry("models/windmill.obj"));
  meshes["windmill"].get_transform().scale = vec3(0.04f, 0.04f, 0.04f);
  meshes["windmill"].get_transform().rotate(vec3(0, pi<float>(), 0));
  meshes["windmill"].get_transform().translate(vec3(-25.0f, 0.0f, 50.0f));

  meshes["ufo"] = mesh(geometry("models/ufo.obj"));
  meshes["ufo"].get_transform().scale = vec3(0.6f, 0.6f, 0.6f);
  meshes["ufo"].get_transform().translate(vec3(35.0f, 40.0f, 50.0f));
  meshes["ufo"].get_transform().rotate(vec3(0.2*(half_pi<float>()), 0.0f, 0.0f));

  // Fireworks meshes.
  meshes["k1"] = mesh(geometry_builder::create_box(vec3(0.1f, 6.0f, 0.1f)));
  meshes["k1"].get_transform().translate(vec3(2.0f, 0.0f, 0.0f));
  explode_meshes["f1"] = mesh(geometry_builder::create_sphere(20.0f, 20.0f));
  explode_meshes["f1"].get_transform().translate(vec3(2.0f, 3.0f, 0.0f));
  explode_meshes["f1"].get_transform().scale = vec3(0.5f, 0.5f, 0.5f);
  meshes["k2"] = mesh(geometry_builder::create_box(vec3(0.1f, 6.0f, 0.1f)));
  meshes["k2"].get_transform().translate(vec3(-2.0f, 0.0f, 1.0f));
  explode_meshes["f2"] = mesh(geometry_builder::create_sphere(20.0f, 20.0f));
  explode_meshes["f2"].get_transform().translate(vec3(-2.0f, 3.0f, 1.0f));
  explode_meshes["f2"].get_transform().scale = vec3(0.5f, 0.5f, 0.5f);
  meshes["k3"] = mesh(geometry_builder::create_box(vec3(0.1f, 6.0f, 0.1f)));
  meshes["k3"].get_transform().translate(vec3(0.0f, 0.0f, 3.0f));
  explode_meshes["f3"] = mesh(geometry_builder::create_sphere(20.0f, 20.0f));
  explode_meshes["f3"].get_transform().translate(vec3(0.0f, 3.0f, 3.0f));
  explode_meshes["f3"].get_transform().scale = vec3(0.5f, 0.5f, 0.5f);
  meshes["k4"] = mesh(geometry_builder::create_box(vec3(0.1f, 6.0f, 0.1f)));
  meshes["k4"].get_transform().translate(vec3(3.0f, 0.0f, -2.0f));
  explode_meshes["f4"] = mesh(geometry_builder::create_sphere(20.0f, 20.0f));
  explode_meshes["f4"].get_transform().translate(vec3(3.0f, 3.0f, -2.0f));
  explode_meshes["f4"].get_transform().scale = vec3(0.5f, 0.5f, 0.5f);
  meshes["k5"] = mesh(geometry_builder::create_box(vec3(0.1f, 6.0f, 0.1f)));
  meshes["k5"].get_transform().translate(vec3(0.0f, 0.0f, 1.0f));
  explode_meshes["f5"] = mesh(geometry_builder::create_sphere(20.0f, 20.0f));
  explode_meshes["f5"].get_transform().translate(vec3(0.0f, 3.0f, 1.0f));
  explode_meshes["f5"].get_transform().scale = vec3(0.5f, 0.5f, 0.5f);
  explode_meshes["f6"] = mesh(geometry_builder::create_sphere(20.0f, 20.0f));

  textures["ground"] = texture("textures/ground.jpg");
  textures["rock"] = texture("textures/rock.jpg");
  textures["house"] = texture("textures/house_diffuse.tga");
  textures["windmill"] = texture("textures/windmill_diffuse.tga");
  textures["ufo"] = texture("textures/ufo_diffuse.png");
  textures["ball"] = texture("textures/ball.jpg");
  textures["red"] = texture("textures/red.jpg");
  textures["green"] = texture("textures/green.jpeg");

  textures_normals["house"] = texture("textures/house_normal.tga");
  textures_normals["windmill"] = texture("textures/windmill_normal.tga");
  textures_normals["ufo"] = texture("textures/ufo_normal.png");

  array<string, 6> filenames = { "textures/stars.jpg", "textures/stars.jpg", "textures/stars.jpg",
	  "textures/stars.jpg", "textures/stars.jpg", "textures/stars.jpg" };

  cube_map = cubemap(filenames);

  // Lighting settings.
  light.set_ambient_intensity(vec4(0.8f, 0.8f, 0.8f, 1.0f));
  light.set_light_colour(vec4(1.0f, 1.0f, 1.0f, 1.0f));
  light.set_direction(normalize(- meshes["windmill"].get_transform().position));

  spot.set_position(vec3(2500.0f, -6000.0f, -50.0f));
  spot.set_light_colour(vec4(1.0f, 1.0f, 1.0f, 1.0f));
  spot.set_range(200.0f);
  spot.set_power(0.8f);
  spot.set_direction(normalize(-spot.get_position()));

  // Effects creation, shaders adding.
  eff.add_shader("shaders/basic.vert", GL_VERTEX_SHADER);
  vector<string> frag_shaders{ "shaders/basic.frag", "shaders/part_direction.frag", "shaders/part_normal_map.frag", "shaders/part_shadow.frag", "shaders/part_spot.frag"};
  eff.add_shader(frag_shaders, GL_FRAGMENT_SHADER);
  eff.build();

  mblur.add_shader("shaders/simple_texture.vert", GL_VERTEX_SHADER);
  mblur.add_shader("shaders/mblur.frag", GL_FRAGMENT_SHADER);
  mblur.build();

  explode.add_shader("shaders/shader.vert", GL_VERTEX_SHADER);
  explode.add_shader("shaders/shader.frag", GL_FRAGMENT_SHADER);
  explode.add_shader("shaders/explode.geom", GL_GEOMETRY_SHADER);
  explode.build();

  ter_eff.add_shader("shaders/terrain.vert", GL_VERTEX_SHADER);
  ter_eff.add_shader("shaders/terrain.frag", GL_FRAGMENT_SHADER);
  ter_eff.add_shader("shaders/part_direction.frag", GL_FRAGMENT_SHADER);
  ter_eff.add_shader("shaders/part_weighted_texture_4.frag", GL_FRAGMENT_SHADER);
  ter_eff.build();

  sky_eff.add_shader("shaders/skybox.vert", GL_VERTEX_SHADER);
  sky_eff.add_shader("shaders/skybox.frag", GL_FRAGMENT_SHADER);
  sky_eff.build();

  smoke_eff.add_shader("shaders/smoke.vert", GL_VERTEX_SHADER);
  smoke_eff.add_shader("shaders/smoke.frag", GL_FRAGMENT_SHADER);
  smoke_eff.add_shader("shaders/smoke.geom", GL_GEOMETRY_SHADER);
  smoke_eff.build();

  smoke_compute_eff.add_shader("shaders/particle.comp", GL_COMPUTE_SHADER);
  smoke_compute_eff.build();

  shadow_eff.add_shader("shaders/basic.vert", GL_VERTEX_SHADER);
  shadow_eff.add_shader(frag_shaders, GL_FRAGMENT_SHADER);
  shadow_eff.build();

  tex_eff.add_shader("shaders/simple_texture.vert", GL_VERTEX_SHADER);
  tex_eff.add_shader("shaders/simple_texture.frag", GL_FRAGMENT_SHADER);
  tex_eff.build();

  // Vao we need to avoid errors.
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  glGenBuffers(1, &G_Position_buffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, G_Position_buffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * MAX_PARTICLES, positionsSM, GL_DYNAMIC_DRAW);
  glGenBuffers(1, &G_Velocity_buffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, G_Velocity_buffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * MAX_PARTICLES, velocitysSM, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  // Setting cameras properties - free camera 1, target camera, chase camera and free camera 2.
  cam.set_position(vec3(0.0f, 10.0f, 0.0f));
  cam.set_target(vec3(0.0f, 0.0f, 0.0f));
  cam.set_projection(quarter_pi<float>(), renderer::get_screen_aspect(), 0.1f, 1000.0f);

  cam_t.set_position(vec3(40.0f, 50.0f, 50.0f));
  cam_t.set_target(vec3(0.0f, 0.0f, 0.0f));
  cam_t.set_projection(quarter_pi<float>(), renderer::get_screen_aspect(), 0.1f, 1000.0f);

  cam_c.set_pos_offset(vec3(0.0f, 2.0f, 10.0f));
  cam_c.set_springiness(1.0f);
  cam_c.move(meshes["ufo"].get_transform().position, eulerAngles(meshes["house"].get_transform().orientation));
  cam_c.set_projection(quarter_pi<float>(), renderer::get_screen_aspect(), 0.1f, 1000.0f);

  auto aspect = static_cast<float>(renderer::get_screen_width()) / static_cast<float>(renderer::get_screen_height());
  tcam2.set_position(vec3(10.0f, 0.0f, 20.0f));
  tcam2.set_target(vec3(0.0f, 0.0f, 0.0f));
  tcam2.set_projection(quarter_pi<float>(), aspect, 2.414f, 1000.0f);

  return true;
}


bool update(float delta_time) {

	// Fireworks
	if (glfwGetKey(renderer::get_window(), 'H'))
		fireworks_start = 1;

	vec3 zmi9;
	if (fireworks_start == 1 && explode_meshes["f1"].get_transform().position.y < 70)
	{
		zmi9 += vec3(0.0f, 2.0f, 0.0f);
		meshes["k1"].get_transform().translate(zmi9);
		meshes["k2"].get_transform().translate(zmi9);
		meshes["k3"].get_transform().translate(zmi9);
		meshes["k4"].get_transform().translate(zmi9);
		meshes["k5"].get_transform().translate(zmi9);
		explode_meshes["f1"].get_transform().translate(zmi9);
		explode_meshes["f2"].get_transform().translate(zmi9);
	    explode_meshes["f3"].get_transform().translate(zmi9);
		explode_meshes["f4"].get_transform().translate(zmi9);
		explode_meshes["f5"].get_transform().translate(zmi9);
	}
	else if (fireworks_start == 1 && explode_meshes["f1"].get_transform().position.y >= 70)
	{ 
		explode_f -= 0.5f;
		zmi9 += vec3(0.0f, -1.5f, 0.0f);
		meshes["k1"].get_transform().position = vec3(zmi9);
		meshes["k2"].get_transform().position = vec3(zmi9);
		meshes["k3"].get_transform().position = vec3(zmi9);
		meshes["k4"].get_transform().position = vec3(zmi9);
		meshes["k5"].get_transform().position = vec3(zmi9);
	}

	if (explode_f < -30)
	{
		explode_f = 0;
		meshes["k1"].get_transform().position = vec3(2.0f, 0.0f, 0.0f);
		meshes["k2"].get_transform().position = vec3(-2.0f, 0.0f, 1.0f);
		meshes["k3"].get_transform().position = vec3(2.0f, 0.0f, 3.0f);
		meshes["k4"].get_transform().position = vec3(3.0f, 0.0f, -2.0f);
		meshes["k5"].get_transform().position = vec3(2.0f, 0.0f, 3.0f);
		explode_meshes["f1"].get_transform().position = vec3(2.0f, 3.0f, 0.0f);
	    explode_meshes["f2"].get_transform().position = vec3(-2.0f, 3.0f, 1.0f);
		explode_meshes["f3"].get_transform().position = vec3(2.0f, 3.0f, 3.0f);
		explode_meshes["f4"].get_transform().position = vec3(3.0f, 3.0f, -2.0f);
		explode_meshes["f5"].get_transform().position = vec3(2.0f, 3.0f, 3.0f);
		fireworks_start = 1;
	}

	// Looking from the outside of the world - options.
	if (glfwGetKey(renderer::get_window(), 'T'))
		render_out = 1;

	if (glfwGetKey(renderer::get_window(), 'U'))
		prevpos = tcam2.get_position() + vec3(0.0f, 0.0f, -5.0f);
	
	if (glfwGetKey(renderer::get_window(), 'I'))
		prevpos = tcam2.get_position() - vec3(0.0f, 0.0f, -5.0f);
		
	tcam2.set_position(prevpos);

	if (glfwGetKey(renderer::get_window(), 'Y'))
		render_out = 0;

	tcam2.update(delta_time);

	// Bomb dropping.
	if (glfwGetKey(renderer::get_window(), 'E'))
		bomb = 1;
	if (glfwGetKey(renderer::get_window(), 'R'))
		bomb = 0;

	if (bomb == 0)
		meshes["s4"].get_transform().position = meshes["ufo"].get_transform().position;
	else
		meshes["s4"].get_transform().translate(vec3(0.0f, -10.0f, 0.0f));

	vec3 zmi2;

	if (meshes["s4"].get_transform().position.y < 5 && meshes["s4"].get_transform().position.y > -200)
		spot.set_position(vec3(meshes["s4"].get_transform().position.x, 10.0, meshes["s4"].get_transform().position.z));

	if (meshes["s4"].get_transform().position.y < 5 && meshes["s4"].get_transform().position.y > -1000)
	{
		float f1 = (float)dist(e1);
		zmi2 += vec3(f1, f1, f1) * delta_time;
		terr.get_transform().translate(zmi2);
		meshes["windmill"].get_transform().translate(zmi2);
		meshes["house"].get_transform().translate(zmi2);
		explode_pos.x = meshes["s4"].get_transform().position.x;
		explode_pos.y = 10.0;
		explode_pos.z = meshes["s4"].get_transform().position.z;
		ilumination += 0.5;
	}
	else if(ilumination != 1)
		ilumination -= 0.5;

	if (meshes["s4"].get_transform().position.y < 5)
	{
		vec3 zmi3;
		zmi3 += vec3(120, 120, 120) * delta_time;
		meshes["ufo"].get_transform().translate(zmi3);
		stop_moving_ufo = 1;
		meshes["ufo"].get_transform().rotate(vec3(2.5*(half_pi<float>()), 1.5*(half_pi<float>()), 3.5*(half_pi<float>())) * delta_time);
	}

	if (meshes["ufo"].get_transform().position.y < 0)
		meshes["ufo"].get_transform().position.y += 100;


	// Smoke
	if (delta_time > 10.0f) {
		delta_time = 10.0f;
	}
	renderer::bind(smoke_compute_eff);
	glUniform3fv(smoke_compute_eff.get_uniform_location("max_dims"), 1, &(vec3(3.0f, 5.0f, 5.0f))[0]);
	glUniform1f(smoke_compute_eff.get_uniform_location("delta_time"), delta_time);

	current_frame = (current_frame + 1) % 2;

	if (glfwGetKey(renderer::get_window(), 'B'))
		mblur_on = 1;
	if (glfwGetKey(renderer::get_window(), 'N'))
		mblur_on = 0;

	// The earthquake effect.
	if (glfwGetKey(renderer::get_window(), 'M')) 
	{ 
		float f1 = (float)dist(e1);
		zmi2 += vec3(f1, f1, f1) * delta_time;
		terr.get_transform().translate(zmi2);
		meshes["windmill"].get_transform().translate(zmi2);
		meshes["house"].get_transform().translate(zmi2);
		zmi5 -= vec3(0.0f, 0.0005f, 0.0f);
		meshes["windmill"].get_transform().translate(zmi5);
		meshes["house"].get_transform().translate(zmi5);
	}

	// Camera choosing. "control_on" is an option of UFO vehicle controlling.
	if (glfwGetKey(renderer::get_window(), '1')) { 
		chosen_camera = 1; 
		control_on = 0;
	}

	if (glfwGetKey(renderer::get_window(), '2')) { 
		chosen_camera = 2; 
		cam_t.set_position(vec3(-60.0f, 25.0f, 70.0f));
		control_on = 0;
	}

	if (glfwGetKey(renderer::get_window(), '3')) {
		chosen_camera = 3;
		cam_t.set_position(vec3(60.0f, 20.0f, -50.0f));
		control_on = 0;
	}

	if (glfwGetKey(renderer::get_window(), '4')) {
		chosen_camera = 4;
	}

	// The ratio of pixels to rotation
	static double ratio_width = quarter_pi<float>() / static_cast<float>(renderer::get_screen_width());
	static double ratio_height =
		(quarter_pi<float>() *
		(static_cast<float>(renderer::get_screen_height()) / static_cast<float>(renderer::get_screen_width()))) /
		static_cast<float>(renderer::get_screen_height());

	double current_x;
	double current_y;
	
	// Getting the current cursor position
	glfwGetCursorPos(renderer::get_window(), &current_x, &current_y);

	// Calculating delta of cursor positions from last frame
	double delta_x = current_x - cursor_x;
	double delta_y = current_y - cursor_y;

	// Multiplying deltas by ratios for mouse control
	delta_x = delta_x * ratio_width;
	delta_y = delta_y * ratio_height;

	// Cameras rotating
	if (chosen_camera == 1)
		cam.rotate(delta_x, -delta_y);

	else if (chosen_camera == 4)
		cam_c.rotate(vec3(-delta_y, -delta_x, 0.0f));
	
	// Using keyboard to move the camera - WSAD
	vec3 zmi;

	if (glfwGetKey(renderer::get_window(), 'W')) { zmi += vec3(0.0f, 0.0f, 30.0f) * delta_time; }
	if (glfwGetKey(renderer::get_window(), 'S')) { zmi += vec3(0.0f, 0.0f, -30.0f) * delta_time; }
	if (glfwGetKey(renderer::get_window(), 'A')) { zmi += vec3(-30.0f, 0.0f, 0.0f) * delta_time; }
    if (glfwGetKey(renderer::get_window(), 'D')) { zmi += vec3(30.0f, 0.0f, 0.0f) * delta_time; }

	// Moving the camera
	if (chosen_camera == 1)
		cam.move(zmi);

	// Updating cursor position
	cursor_x = current_x;
	cursor_y = current_y;
	
	// Passing light direction and position into the shadow.
	shadow.light_position = spot.get_position();
	shadow.light_dir = spot.get_direction();

	// Cameras updating
	if (chosen_camera == 1)
		cam.update(delta_time); 
	else if (chosen_camera == 2 || chosen_camera == 3)
		cam_t.update(delta_time);
	else if (chosen_camera ==4)
	{
		cam_c.move(meshes["ufo"].get_transform().position - vec3(-90, -35, 0), eulerAngles(meshes["house"].get_transform().orientation));
		cam_c.update(delta_time);
	}

	// Rotating the UFO vehicle
	meshes["ufo"].get_transform().rotate(vec3(0.0f, 1.5*(half_pi<float>()), 0.0f) * delta_time);
	t += 0.01;
	if (control_on == 0 && stop_moving_ufo == 0)
		meshes["ufo"].get_transform().position = vec3( cos(t)*200.0f, 70, sin(t)*200.0f);

	// Rotating the spheres
		meshes["s1"].get_transform().position = vec3(cos(t * 2)*300.0f, 180, sin(t * 2)*300.0f);
		meshes["s2"].get_transform().position = vec3(cos(t*1.3)*200.0f + 200, 230, sin(t*1.3)*200.0f + 130);
		meshes["s3"].get_transform().position = vec3(cos(t * 1.8)*1600.0f, 320, sin(t * 1.8)*160.0f);

	// Moving (controlling) the UFO vehicle (only on chase camera (number 4)).
	vec3 mov;
	float speed = 60;

	if (chosen_camera == 4 && glfwGetKey(renderer::get_window(), GLFW_KEY_LEFT))
	{ 
		control_on = 1;
		mov += vec3(0.0f, 0.0f, speed) * delta_time; 
	}
	if (chosen_camera == 4 &&  glfwGetKey(renderer::get_window(), GLFW_KEY_RIGHT))
	{ 
		control_on = 1;
		mov += vec3(0.0f, 0.0f, -speed) * delta_time;
	}
	if (chosen_camera == 4 &&  glfwGetKey(renderer::get_window(), GLFW_KEY_UP))
	{ 
		control_on = 1;
		mov += vec3(-speed, 0.0f, 0.0f) * delta_time;
	}
	if (chosen_camera == 4 &&  glfwGetKey(renderer::get_window(), GLFW_KEY_DOWN))
	{ 
		control_on = 1;
		mov += vec3(speed, 0.0f, 0.0f) * delta_time;
	}
	if (chosen_camera == 4 && glfwGetKey(renderer::get_window(), 'Z'))
	{
		mov += vec3(0.0f, speed, 0.0f) * delta_time;
	}
	if (chosen_camera == 4 && glfwGetKey(renderer::get_window(), 'X'))
	{
		mov += vec3(0.0f, -speed, 0.0f) * delta_time;
	}
	
	meshes["ufo"].get_transform().translate(mov);
	
	return true;
}

bool render() {

	// Shadows rendering
	renderer::set_render_target(shadow);
	glClear(GL_DEPTH_BUFFER_BIT);
	glCullFace(GL_FRONT);
	mat4 LightProjectionMat = perspective<float>(90.f, renderer::get_screen_aspect(), 0.1f, 1000.f);
	renderer::bind(shadow_eff);
	auto V = shadow.get_view();

	for (auto &e : meshes)
	{
		auto m = e.second;
		auto M = m.get_transform().get_transform_matrix();
		auto MVP = LightProjectionMat * V * M;
		glUniformMatrix4fv(shadow_eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
		renderer::render(m);
	}

	renderer::set_render_target();
	glCullFace(GL_BACK);
	
	// Frame buffer for the outside look - first pass.
	if (mblur_on == 0)
	{
		renderer::set_render_target(tframe);
		renderer::clear();
	}

	// Frame buffer for the blur moving effect - first pass.
	if (mblur_on == 1)
	{
		renderer::set_render_target(temp_frame);
		renderer::clear();
	}

	// Skybox rendering
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	renderer::bind(sky_eff);
	auto M = skybox.get_transform().get_transform_matrix();
	V = cam.get_view();
	auto P = cam.get_projection();

	if (chosen_camera == 2 || chosen_camera == 3)
	{
		V = cam_t.get_view();
		P = cam_t.get_projection();
	}
	else if (chosen_camera == 4)
	{
		V = cam_c.get_view();
		P = cam_c.get_projection();
	}

	auto MVP = P * V * M;
	glUniformMatrix4fv(sky_eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
	renderer::render(skybox);
	renderer::bind(cube_map, 0);
	glUniform1i(sky_eff.get_uniform_location("cubemap"), 0);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);

	// Terrain rendering.
	renderer::bind(ter_eff);
	auto Ma = terr.get_transform().get_transform_matrix();
	mat4x4 Va;
	mat4x4 Pa;
	if (chosen_camera == 1)
	{
		Va = cam.get_view();
		Pa = cam.get_projection();
	}
	if (chosen_camera == 2 || chosen_camera == 3)
	{
		Va = cam_t.get_view();
		Pa = cam_t.get_projection();
	}
	else if (chosen_camera == 4)
	{
		Va = cam_c.get_view();
		Pa = cam_c.get_projection();
	}
	auto MVPa = Pa * Va * Ma;
	glUniformMatrix4fv(ter_eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVPa));
	glUniformMatrix4fv(ter_eff.get_uniform_location("M"), 1, GL_FALSE, value_ptr(M));
	glUniformMatrix3fv(ter_eff.get_uniform_location("N"), 1, GL_FALSE, value_ptr(terr.get_transform().get_normal_matrix()));
	glUniform3fv(ter_eff.get_uniform_location("eye_pos"), 1, value_ptr(cam.get_position()));
	renderer::bind(terr.get_material(), "mat");
	renderer::bind(light, "light");
	renderer::bind(tex[0], 0);
	glUniform1i(ter_eff.get_uniform_location("tex[0]"), 0);
	renderer::bind(tex[1], 1);
	glUniform1i(ter_eff.get_uniform_location("tex[1]"), 1);
	renderer::bind(tex[2], 2);
	glUniform1i(ter_eff.get_uniform_location("tex[2]"), 2);
	renderer::bind(tex[3], 3);
	glUniform1i(ter_eff.get_uniform_location("tex[3]"), 3);
	renderer::render(terr);

	// Rendering meshes with simple texturing.
	if (chosen_camera == 1)
	{
		P = cam.get_projection();
		V = cam.get_view();
	}
	else if (chosen_camera == 2 || chosen_camera == 3)
	{
		P = cam_t.get_projection();
		V = cam_t.get_view();
	}
	else if (chosen_camera == 4)
	{
		P = cam_c.get_projection();
		V = cam_c.get_view();
	}
	auto Vsh = shadow.get_view();

	for (auto &e : meshes)
	{
		auto m = e.second;
		renderer::bind(eff);
		mat4 M = m.get_transform().get_transform_matrix();

		auto MVP = P * V * M;
		glUniformMatrix4fv(eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
		glUniformMatrix4fv(eff.get_uniform_location("M"), 1, GL_FALSE, value_ptr(M));
		glUniformMatrix3fv(eff.get_uniform_location("N"), 1, GL_FALSE, value_ptr(m.get_transform().get_normal_matrix()));
		glUniform1f(eff.get_uniform_location("ilumination"), ilumination);

		// Shadowing part.
		M = m.get_transform().get_transform_matrix();
		auto lightMVP = LightProjectionMat * Vsh * M;
		glUniformMatrix4fv(eff.get_uniform_location("lightMVP"), 1, GL_FALSE, value_ptr(lightMVP));
		renderer::bind(m.get_material(), "mat");
		renderer::bind(light, "light");
		renderer::bind(spot, "spot");

		// Binding appropriate textures to meshes.
		if (e.first == "moon")
		{
			renderer::bind(textures["rock"], 0);
			glUniform1i(eff.get_uniform_location("tex"), 0);
		}

		if (e.first == "s1")
			renderer::bind(textures["red"], 0);

		if (e.first == "s2")
			renderer::bind(textures["red"], 0);

		if (e.first == "s3")
			renderer::bind(textures["red"], 0);

		if (e.first == "s4")
			renderer::bind(textures["green"], 0);
			
		if (e.first == "sx")
			renderer::bind(textures["green"], 0);

		if (e.first == "house")
		{
			renderer::bind(textures["house"], 0);
			renderer::bind(textures_normals["house"], 3);
			glUniform1i(eff.get_uniform_location("normal_map"), 3);
		}

		else if (e.first == "windmill")
		{
			renderer::bind(textures["windmill"], 0);
			renderer::bind(textures_normals["windmill"], 3);
		}

		else if (e.first == "ufo")
		{
			renderer::bind(textures["ufo"], 0);
			renderer::bind(textures_normals["ufo"], 3);
		}

		// Passing the camera position into a shader as an eye position.
		if (chosen_camera == 4)
			glUniform3fv(eff.get_uniform_location("eye_pos"), 1, value_ptr(cam_c.get_position()));
		else
			glUniform3fv(eff.get_uniform_location("eye_pos"), 1, value_ptr(cam.get_position()));

		// Binding the shadow map with the shader.
		renderer::bind(shadow.buffer->get_depth(), 1);
		glUniform1i(eff.get_uniform_location("shadow_map"), 1);

		renderer::render(m);
	}


	if (mblur_on == 1)
	{
		// SECOND PASS
		renderer::set_render_target(frames[current_frame]);
		renderer::clear();
		renderer::bind(mblur);
		MVP = mat4x4();
		glUniformMatrix4fv(mblur.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
		renderer::bind(temp_frame.get_frame(), 0);
		renderer::bind(frames[(current_frame + 1) % 2].get_frame(), 1);
		glUniform1i(mblur.get_uniform_location("previous_frame"), 1);
		glUniform1i(mblur.get_uniform_location("tex"), 0);
		glUniform1f(mblur.get_uniform_location("blend_factor"), 0.9f);
		renderer::render(screen_quad);

		// SCREEN PASS
		renderer::set_render_target();
		renderer::bind(tex_eff);
		glUniformMatrix4fv(tex_eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
		renderer::bind(frames[current_frame].get_frame(), 0);
		glUniform1i(tex_eff.get_uniform_location("tex"), 0);
		renderer::render(screen_quad);
	}


	// Explosion
	V = cam.get_view();
	P = cam.get_projection();
	for (auto &e : explode_meshes)
	{
		auto m = e.second;
		renderer::bind(explode);
		M = m.get_transform().get_transform_matrix();
		if (chosen_camera == 2 || chosen_camera == 3)
		{
			V = cam_t.get_view();
			P = cam_t.get_projection();
		}
		else if (chosen_camera == 4)
		{
			V = cam_c.get_view();
			P = cam_c.get_projection();
		}
		MVP = P * V * M;
		glUniformMatrix4fv(explode.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
		glUniformMatrix3fv(explode.get_uniform_location("N"), 1, GL_FALSE, value_ptr(m.get_transform().get_normal_matrix()));
		glUniform1f(explode.get_uniform_location("explode_factor"), explode_f);
		renderer::render(m);
	}

	// Smoke effect.
	if (render_out == 0)
	{
		renderer::bind(smoke_compute_eff);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, G_Position_buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, G_Velocity_buffer);
		glDispatchCompute(MAX_PARTICLES / 128, 1, 1);
		glMemoryBarrier(GL_ALL_BARRIER_BITS);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		renderer::bind(smoke_eff);
		mat4 Ms(1.0f);
		//Ms = translate(mat4(1.0f), vec3(10, 10, 10));
		//Ms = translate(mat4(1.0f), explode_pos);
		auto Vs = cam.get_view();
		auto MVs = Ms * Vs;
		glUniform4fv(smoke_eff.get_uniform_location("colour"), 1, value_ptr(vec4(1.0f)));
		glUniformMatrix4fv(smoke_eff.get_uniform_location("MV"), 1, GL_FALSE, value_ptr(MVs));
		auto Ps = cam.get_projection();
		glUniformMatrix4fv(smoke_eff.get_uniform_location("P"), 1, GL_FALSE, value_ptr(Ps));
		glUniform1f(smoke_eff.get_uniform_location("point_size"), 0.1f);
		renderer::bind(smoke_tex, 0);
		glUniform1i(smoke_eff.get_uniform_location("tex"), 0);
		glBindBuffer(GL_ARRAY_BUFFER, G_Position_buffer);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, (void *)0);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);
		glDrawArrays(GL_POINTS, 0, MAX_PARTICLES);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
		glDisableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glUseProgram(0);
	}

	//FBUFFER - screen pass.
	if (mblur_on == 0)
	{
		for (int i = 0; i < 2; i++)
		{
			renderer::setClearColour(0.0f, 0.0f, 0.0f);
			renderer::set_render_target();
			renderer::bind(tex_eff);
			if (render_out == 1)
			{
				M = render_cube.get_transform().get_transform_matrix();
				V = tcam2.get_view();
				P = tcam2.get_projection();
				MVP = P * V * M;
			}
			else
				MVP = mat4x4();

			glUniformMatrix4fv(tex_eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
			renderer::bind(tframe.get_frame(), 0);
			glUniform1i(tex_eff.get_uniform_location("tex"), 0);

			if (render_out == 1)
				renderer::render(meshes_out["q1"]);
			else
				renderer::render(screen_quad);
		}
	}

  return true;
}

void main() {
  // Create application
  app application("Graphics Coursework");
  // Set load content, update and render methods
  application.set_load_content(load_content);
  application.set_initialise(initialise);
  application.set_update(update);
  application.set_render(render);
  // Run application
  application.run();
}