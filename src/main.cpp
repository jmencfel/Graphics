#include <glm\glm.hpp>
#include <graphics_framework.h>

using namespace std;
using namespace graphics_framework;
using namespace glm;

geometry geom;
effect eff, sky_eff, motion_blur, tex_eff, liquid_eff;

camera* cam[2];
camera* reflection_cam; 
int current_camera = 0;

float click_delay;
float glitch_delay;
float time_delta;
float time_waves;
double cursor_x = 0.0;
double cursor_y = 0.0;
bool show_FPS=false;
bool show_normal_maps = true;

map<string, mesh> meshes;
map<string, mesh>shadow_meshes;
map<string, texture> textures;
map<string, texture> normal_maps;

shadow_map shadow;
array<mesh, 3> hier_meshes;

//liquid in the volcano
mesh liquid;
texture liquid_tex;
texture liquid_norm_map;


frame_buffer frames[2];
frame_buffer temp_frame;
frame_buffer reflection; //water reflection

////visual effects booleans
bool fisheye = false;
bool grayscale = false;
bool LSD = false;
bool help = false;
bool chroma_ab = false;
bool tv_noise = false;
bool glitches = true;
bool show_masks = true;

float blend_factor = 0.55f;

unsigned int current_frame = 0;
geometry screen_quad;


directional_light light;
vector<point_light> points(10);
vector<spot_light> spots(2);

material mat;

cubemap cube_map;
mesh skybox;


bool initialise() {
	cam[0] = new free_camera();
	cam[1] = new target_camera();
	reflection_cam = new target_camera();

	//hide cursor, capture initial mouse position (for free camera)
	glfwSetInputMode(renderer::get_window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwGetCursorPos(renderer::get_window(), &cursor_x, &cursor_y);
	return true;
}

void generate_terrain(geometry &geom, const texture &height_map, unsigned int width, unsigned int depth, float height_scale) {
	// Contains our position data
	vector<vec3> positions;
	// Contains our normal data
	vector<vec3> normals;
	// Contains our texture coordinate data
	vector<vec2> tex_coords;
	// Contains our texture weights
	vector<vec4> tex_weights;
	// Contains our index data
	vector<unsigned int> indices;

	// Extract the texture data from the image
	glBindTexture(GL_TEXTURE_2D, height_map.get_id());
	auto data = new vec4[height_map.get_width() * height_map.get_height()];
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, (void *)data);

	// Determine ratio of height map to geometry
	float width_point = static_cast<float>(width) / static_cast<float>(height_map.get_width());
	float depth_point = static_cast<float>(depth) / static_cast<float>(height_map.get_height());

	// Point to work on
	vec3 point;

	// Part 1 - Iterate through each point, calculate vertex and add to vector
	for (int x = 0; x < height_map.get_width(); ++x) {
		// Calculate x position of point
		point.x = -(width / 2.0f) + (width_point * static_cast<float>(x));

		for (int z = 0; z < height_map.get_height(); ++z) {
			// *********************************
			// Calculate z position of point
			point.z = -(depth / 2.0f) + (depth_point * z);
			// *********************************
			// Y position based on red component of height map data
			point.y = data[(z * height_map.get_width()) + x].y * height_scale;
			point.y -= distance(vec2(point.x, point.z), vec2(0, 0))/2.0f;
			// Add point to position data
			positions.push_back(point);
		}
	}

	// Part 1 - Add index data
	for (unsigned int x = 0; x < height_map.get_width() - 1; ++x) {
		for (unsigned int y = 0; y < height_map.get_height() - 1; ++y) {
			// Get four corners of patch
			unsigned int top_left = (y * height_map.get_width()) + x;
			unsigned int top_right = (y * height_map.get_width()) + x + 1;
			// *********************************
			unsigned int bottom_left = ((y + 1) * height_map.get_width()) + x;
			unsigned int bottom_right = ((y + 1) * height_map.get_height()) + x + 1;
			// *********************************
			// Push back indices for triangle 1 (tl,br,bl)
			indices.push_back(top_left);
			indices.push_back(bottom_right);
			indices.push_back(bottom_left);
			// Push back indices for triangle 2 (tl,tr,br)
			// *********************************
			indices.push_back(top_left);
			indices.push_back(top_right);
			indices.push_back(bottom_right);
			// *********************************
		}
	}

	// Resize the normals buffer
	normals.resize(positions.size());

	// Part 2 - Calculate normals for the height map
	for (unsigned int i = 0; i < indices.size() / 3; ++i) {
		// Get indices for the triangle
		auto idx1 = indices[i * 3];
		auto idx2 = indices[i * 3 + 1];
		auto idx3 = indices[i * 3 + 2];

		// Calculate two sides of the triangle
		vec3 side1 = positions[idx1] - positions[idx3];
		vec3 side2 = positions[idx1] - positions[idx2];

		// Normal is normal(cross product) of these two sides
		// *********************************
		vec3 normal = cross(side2, side1);

		// Add to normals in the normal buffer using the indices for the triangle
		normals[idx1] = normalize(normals[idx1] + normal);
		normals[idx2] = normalize(normals[idx2] + normal);
		normals[idx3] = normalize(normals[idx3] + normal);
		// *********************************
	}
	// Part 3 - Add texture coordinates for geometry
	for (unsigned int x = 0; x < height_map.get_width(); ++x) {
		for (unsigned int z = 0; z < height_map.get_height(); ++z) {
			tex_coords.push_back(vec2(width_point * x, depth_point * z));
		}
	}

	// Part 4 - Calculate texture weights for each vertex
	for (unsigned int x = 0; x < height_map.get_width(); ++x) {
		for (unsigned int z = 0; z < height_map.get_height(); ++z) {
			// Calculate tex weight
			vec4 tex_weight(clamp(1.0f - abs(data[(height_map.get_width() * z) + x].y - 0.0f) / 0.25f, 0.0f, 1.0f),
				clamp(1.0f - abs(data[(height_map.get_width() * z) + x].y - 0.15f) / 0.25f, 0.0f, 1.0f),
				clamp(1.0f - abs(data[(height_map.get_width() * z) + x].y - 0.5f) / 0.25f, 0.0f, 1.0f),
				clamp(1.0f - abs(data[(height_map.get_width() * z) + x].y - 0.9f) / 0.25f, 0.0f, 1.0f));

			// *********************************
			// Sum the components of the vector
			float total = tex_weight.x + tex_weight.y + tex_weight.z + tex_weight.w;
			// Divide weight by sum
			tex_weight = tex_weight / total;
			// Add tex weight to weights
			tex_weights.push_back(tex_weight);
			// *********************************
		}
	}

	// Add necessary buffers to the geometry
	geom.add_buffer(positions, BUFFER_INDEXES::POSITION_BUFFER);
	geom.add_buffer(normals, BUFFER_INDEXES::NORMAL_BUFFER);
	geom.add_buffer(tex_coords, BUFFER_INDEXES::TEXTURE_COORDS_0);
	geom.add_buffer(tex_weights, BUFFER_INDEXES::TEXTURE_COORDS_1);
	geom.add_index_buffer(indices);

	// Delete data
	delete[] data;
}
bool load_skybox() // this method takes care of the skybox
{
	//creating skybox mesh and scaling by 100
	skybox = mesh(geometry_builder::create_box());
	skybox.get_transform().scale *= 100;
	//skybox textures
	array<string, 6> filenames = { "textures/kurt/space_ft.png", "textures/kurt/space_bk.png", "textures/kurt/space_up.png",
									"textures/kurt/space_dn.png", "textures/kurt/space_rt.png", "textures/kurt/space_lf.png" };
	//creating cubemap
	cube_map = cubemap(filenames);
	// Loading shaders
	sky_eff.add_shader("shaders/skybox.vert", GL_VERTEX_SHADER);
	sky_eff.add_shader("shaders/skybox.frag", GL_FRAGMENT_SHADER);
	// building effect
	sky_eff.build();
	return true;
}  
mesh load_terrain() {
	// Geometry to load into
	geometry geom;
	// Load height map
	texture height_map("textures/mountain2.png");
	// Generate terrain
	generate_terrain(geom, height_map, 200, 200, 50.0f);
	// Use geometry to create terrain mesh
	return mesh(geom);
}
//hieratchical meshes
void load_hier()
{

	mat.set_specular(vec4(1.0f, 1.0f, 1.0f, 1.0f));
	mat.set_diffuse(vec4(0.3f, 0.3f, 0.3f, 1.0f));
	mat.set_emissive(vec4(0.05f, 0.05f, 0.05f, 1.0f));
	mat.set_shininess(25);
	textures["rings"] = texture("textures/rings.jpg");


		hier_meshes[0] = mesh(geometry_builder::create_torus(3, 3, 1.4f, 16.0f));
		hier_meshes[0].set_material(mat);
		hier_meshes[1] = mesh(geometry_builder::create_torus(20, 20, 0.7f, 8.0f));
		hier_meshes[1].set_material(mat);
		hier_meshes[2] = mesh(geometry_builder::create_torus(4, 4, 1.4f, 7.0f));
		hier_meshes[2].get_transform().scale = vec3(0.5f,1,1);
		hier_meshes[2].get_transform().rotate(vec3(0,1.0f,0));
		hier_meshes[2].set_material(mat);
//		hier_meshes[i].get_transform().rotate(vec3(half_pi<float>(), 0.0f, 0.0f));
	
	hier_meshes[0].get_transform().translate(vec3(50.0f, 45.0f, 20.0f));


}
//main load func
void load_textures()
{
	normal_maps["blank"] = texture("textures/blank_map.jpg");
	normal_maps["ice_normal"] = texture("textures/ice_normal.jpg");
	normal_maps["surface"] = texture("textures/surface.jpg");
	textures["snow"] = texture("textures/snow2.jpg", true, true);
	textures["gold"] = texture("textures/gold.jpg", true, true);
	normal_maps["rings"] = normal_maps["blank"];
	textures["noise"] = texture("textures/noise.jpg");
	//@@@@	//text on screen in form of a mask 
	textures["mask"] = texture("textures/alpha_map.png");
	textures["camera"] = texture("textures/camera_mask.jpg");
}
bool load_content() {
	//load skybox
	load_skybox();
	//load hierarchical meshes
	load_hier();


	// Create 2 frame buffers - use screen width and height
	frames[0] = frame_buffer(renderer::get_screen_width(), renderer::get_screen_height());
	frames[1] = frame_buffer(renderer::get_screen_width(), renderer::get_screen_height());
	// Create a temp framebuffer
	temp_frame = frame_buffer(renderer::get_screen_width(), renderer::get_screen_height());
	reflection = frame_buffer(renderer::get_screen_width(), renderer::get_screen_height());
	// Create screen quad
	vector<vec3> positions{ vec3(-1.0f, -1.0f, 0.0f), vec3(1.0f, -1.0f, 0.0f), vec3(-1.0f, 1.0f, 0.0f),
		vec3(1.0f, 1.0f, 0.0f) };
	vector<vec2> tex_coords{ vec2(0.0, 0.0), vec2(1.0f, 0.0f), vec2(0.0f, 1.0f), vec2(1.0f, 1.0f) };
	screen_quad.add_buffer(positions, BUFFER_INDEXES::POSITION_BUFFER);
	screen_quad.add_buffer(tex_coords, BUFFER_INDEXES::TEXTURE_COORDS_0);
	screen_quad.set_type(GL_TRIANGLE_STRIP);



	// ********************************
	shadow = shadow_map(renderer::get_screen_width(), renderer::get_screen_height());	
	//predefining repeating textures normal maps under names different than their meshes, so that they 
	//don't get loaded in the program more than once if used multiple names;
	load_textures();


	////////////////////////////MOUNTAIN////////////////////////
	meshes["mountain"] = load_terrain(); //load_terrain returns a mesh
	meshes["mountain"].get_transform().translate(vec3(50.0f, -10.0f, 20.0f));
	meshes["mountain"].get_material().set_diffuse(vec4(0.3f, 0.3f, 0.3f, 1.0f));
	meshes["mountain"].get_material().set_specular(vec4(0.3f, 0.3f, 0.3f, 1.0f));
	meshes["mountain"].get_material().set_shininess(25.0f);
	textures["mountain"] = textures["snow"];
	normal_maps["mountain"] = normal_maps["ice_normal"];

	////////////////////////////ASTEROID////////////////////////
	meshes["asteroid"] = mesh(geometry_builder::create_sphere(100,100));
	meshes["asteroid"].get_material().set_diffuse(vec4(0.15f, 0.15f, 0.15f, 1.0f));
	meshes["asteroid"].get_material().set_specular(vec4(0.3f, 0.3f, 0.3f, 1.0f));
	meshes["asteroid"].get_material().set_shininess(25.0f);
	textures["asteroid"] = textures["snow"];
	normal_maps["asteroid"] = normal_maps["surface"];
	meshes["asteroid"].get_transform().scale *= 300;
	meshes["asteroid"].get_transform().translate(vec3(50.0f, -340.0f, 20.0f));

	////////////////////////////TELESCOPE////////////////////////
	meshes["scope"] = mesh(geometry("models/scope.3DS"));
	meshes["scope"].get_material().set_specular(vec4(1.0f, 1.0f, 1.0f, 1.0f));
	meshes["scope"].get_material().set_shininess(15.0f);
	meshes["scope"].get_material().set_diffuse(vec4(0.3f, 0.3f, 0.3f, 1.0f));
	textures["scope"] = textures["gold"];
	normal_maps["scope"] = normal_maps["ice_normal"];
	meshes["scope"].get_transform().scale *= 0.01;
	meshes["scope"].get_transform().translate(vec3(-50.0f, -54.0f, 20.0f));
	meshes["scope"].get_transform().rotate(vec3(-0.2f,1.7f,0.0f));

	//////////////////////LIQUID INSIDE OF THE VULCANO ///////////////////////////////
	liquid = mesh(geometry_builder::create_plane(10, 10, true));
	liquid.get_transform().translate(vec3(44.0f, 9.5f,25.0f));
	liquid.get_transform().scale *= 3.1f;
	liquid_tex = texture("textures/silver.jpg");
	liquid_norm_map = texture("textures/wave_normal.jpg");
	liquid.get_material().set_diffuse(vec4(0.3f, 0.3f, 0.3f, 1.0f));
	liquid.get_material().set_specular(vec4(0.3f, 0.3f, 0.3f, 1.0f));
	liquid.get_material().set_emissive(vec4(0.05f, 0.05f, 0.05f, 1.0f));
	liquid.get_material().set_shininess(25.0f);

	////////////////////////////MOON////////////////////////
	meshes["moon"] = mesh(geometry_builder::create_sphere(30,30));
	meshes["moon"].get_material().set_specular(vec4(0.0f, 0.0f, 0.0f, 0.0f));
	meshes["moon"].get_material().set_diffuse(vec4(0.3f, 0.3f, 0.3f, 1.0f));
	meshes["moon"].get_material().set_emissive(vec4(0.05f, 0.05f, 0.05f, 1.0f));
	textures["moon"] = texture("textures/moon.jpg");
	normal_maps["moon"] = normal_maps["blank"];
	meshes["moon"].get_transform().translate(vec3(500,500,500));
	meshes["moon"].get_transform().scale *= 50;
	meshes["moon"].get_transform().rotate(vec3(0.0f, 0.0f, 0.0f));

	///////TWO RANDOM OBJECTS TO SEE SHADOWS BETTER
	meshes["rand1"] = mesh(geometry_builder::create_sphere());
	meshes["rand1"].get_transform().translate(vec3(-50.0f, -54.0f, 20.0f));
	textures["rand1"] = textures["rings"];
	normal_maps["rand1"] = normal_maps["ice_normal"];

	meshes["rand2"] = mesh(geometry_builder::create_box());
	meshes["rand2"].get_transform().translate(vec3(-45.0f, -50.0f, 15.0f));
	textures["rand2"] = textures["rings"];
	normal_maps["rand2"] = normal_maps["ice_normal"];

	meshes["spotlight_position"] = mesh(geometry_builder::create_sphere());
	textures["spotlight_position"] = textures["rings"];
	normal_maps["spotlight_position"] = normal_maps["ice_normal"];
	meshes["spotlight_position"].get_material().set_emissive(vec4(0.3f,0,0,1));

	///////////////////////////////LIGHTS////////////////////////////////////////////////////
	//POINT LIGHTS  for testing 
	points[0].set_light_colour(vec4(0, 0, 1, 1));
	points[0].set_position(vec3(-25.0f, 22.0f, -15.0f));
	points[0].set_range(15);

	points[1].set_light_colour(vec4(1, 1, 0, 1));
	points[1].set_position(vec3(-25.0f, 5.0f, -35.0f));
	points[1].set_range(15);

	points[2].set_light_colour(vec4(1, 1, 1, 1));
	points[2].set_position(vec3(5.74f, 21.0f, 0.0f));
	points[2].set_range(4);
//SPOTLIGHTS  (spots[0] is for shadows)

	spots[0].set_light_colour(vec4(1, 0,0, 1));
	spots[0].set_range(150.0f);
	spots[0].set_power(5.4f);

	points[3].set_light_colour(vec4(1, 1, 1, 1));
	points[3].set_position(vec3(-0.5f, 21.0f, 0.0f));
	points[3].set_range(4);


	light.set_ambient_intensity(vec4(0.1f, 0.1f, 0.1f, 1.0f));
	light.set_light_colour(vec4(1, 1, 1, 1));
	light.set_direction(normalize(vec3(0.0f, 1.0f, 0.0f)));

	//"DANCING" rainbow point lights

	points[4].set_light_colour(vec4(1, 0, 0, 1));
	points[4].set_position(vec3(-25.0f, -25.0f, 0.0f));
	points[4].set_range(25);

	points[5].set_light_colour(vec4(1, 1, 0, 1));
	points[5].set_position(vec3(-25.0f, -25.0f, 10.0f));
	points[5].set_range(25);

	points[6].set_light_colour(vec4(0, 1, 0, 1));
	points[6].set_position(vec3(-25.0f, -25.0f, 20.0f));
	points[6].set_range(25);

	points[7].set_light_colour(vec4(0, 1, 1, 1));
	points[7].set_position(vec3(-25.0f, -25.0f, 30.0f));
	points[7].set_range(25);

	points[8].set_light_colour(vec4(0, 0, 1, 1));
	points[8].set_position(vec3(-25.0f, -25.0f, 40.0f));
	points[8].set_range(25);

	points[9].set_light_colour(vec4(1, 0, 1, 1));
	points[9].set_position(vec3(-25.0f, -25.0f,  50.0f));
	points[9].set_range(25);

  // Load in shaders
  eff.add_shader("shaders/shader.vert", GL_VERTEX_SHADER);
  vector<string> frag_shaders{ "shaders/shader.frag", "shaders/part_direction.frag" ,
							   "shaders/part_point.frag" , "shaders/part_spot.frag" ,"shaders/part_normal_map.frag" ,
	                           "shaders/part_shadow.frag"};
  eff.add_shader(frag_shaders, GL_FRAGMENT_SHADER);
 
  vector<string> liquid_frag_shaders{ "shaders/liquid.frag", "shaders/part_direction.frag" ,
										 "shaders/part_normal_map.frag" };
  liquid_eff.add_shader("shaders/liquid.vert", GL_VERTEX_SHADER);
  liquid_eff.add_shader("shaders/liquid.geom", GL_GEOMETRY_SHADER);
  liquid_eff.add_shader(liquid_frag_shaders, GL_FRAGMENT_SHADER);

  tex_eff.add_shader("shaders/simple_texture.vert", GL_VERTEX_SHADER);
  tex_eff.add_shader("shaders/simple_texture.frag", GL_FRAGMENT_SHADER);

  motion_blur.add_shader("shaders/simple_texture.vert", GL_VERTEX_SHADER);
  motion_blur.add_shader("shaders/motion_blur.frag", GL_FRAGMENT_SHADER);


  // Build effect
  eff.build();
  motion_blur.build();
  tex_eff.build();
  liquid_eff.build();
  // Set camera properties
  auto aspect = static_cast<float>(renderer::get_screen_width()) / static_cast<float>(renderer::get_screen_height());
  cam[0]->set_position(vec3(0.0f, 10.0f, 0.0f));
  cam[0]->set_target(vec3(0.0f, 0.0f, 0.0f));
  cam[0]->set_projection(pi<float>()/2.57f, aspect, 0.1f, 1000.0f);

  reflection_cam->set_position(vec3(50.0f, 9.5f, 20.0f));
  reflection_cam->set_target(vec3(50.0f, -6.5f, 20.0f));
  reflection_cam->set_projection(0.55f*pi<float>(), 1.0f, 2.414f, 1000.0f);

  cam[1]->set_position(vec3(-73.0495f, -55.9877f, 17.0259f));
  cam[1]->set_target(vec3(-0.184426f, 7.49289f,  20.6238f));
  cam[1]->set_projection(quarter_pi<float>(), renderer::get_screen_aspect(), 0.1f, 1000.0f);
  return true;
}

bool update(float delta_time) {
	//randomization of visual glitches
	if (glitches && glitch_delay <= 0)
	{
		int R = (rand() % 100 + 1);
		if (R >= 98)
		{
			glitch_delay = 1.0f;
			chroma_ab = true;
			tv_noise = true;
		}
		else
		{
			chroma_ab = false;
			tv_noise = false;
		}
	}
	glitch_delay -= delta_time;
	time_delta += delta_time;
	////time_weaves goes back and forth so that the waves rock about
	time_waves = sin(time_delta);
	current_frame = (current_frame + 1) % 2;
	
	//T turn normal maps ON AND OFF // just for show
	if (click_delay <= 0 && glfwGetKey(renderer::get_window(), GLFW_KEY_T))
	{
		click_delay = 0.3f;
		if (show_normal_maps == false)
		{
			show_normal_maps = true;
			normal_maps["scope"] = normal_maps["ice_normal"];
			normal_maps["asteroid"] = normal_maps["surface"];
			normal_maps["mountain"] = normal_maps["ice_normal"];
		}
		else
		{
			show_normal_maps = false;
			normal_maps["asteroid"] = normal_maps["blank"];
			normal_maps["scope"] = normal_maps["blank"];
			normal_maps["mountain"] = normal_maps["blank"];
		
		}
	}

	if (click_delay <= 0 && (glfwGetKey(renderer::get_window(), GLFW_KEY_L)))
	{
		click_delay = 0.3f;
		fisheye = (fisheye) ? false : true;
	}
	if (click_delay <= 0 && (glfwGetKey(renderer::get_window(), GLFW_KEY_G)))
	{
		click_delay = 0.3f;
		grayscale = (grayscale) ? false : true;
	}
	if (click_delay <= 0 && (glfwGetKey(renderer::get_window(), GLFW_KEY_K)))
	{
		click_delay = 0.3f;
		if (!LSD)
		{		
			blend_factor = 0.9f;
			LSD = true;
		}
		else
		{
			blend_factor = 0.55f;
			LSD = false;
		}
	} 
	//chroma_ab
	if (click_delay <= 0 && (glfwGetKey(renderer::get_window(), GLFW_KEY_M)))
	{
		click_delay = 0.3f;
		chroma_ab = (chroma_ab) ? false : true;
	}
	if (click_delay <= 0 && (glfwGetKey(renderer::get_window(), GLFW_KEY_H)))
	{
		click_delay = 0.3f;
		help = (help) ? false : true;
	}
	if (click_delay <= 0 && (glfwGetKey(renderer::get_window(), GLFW_KEY_J)))
	{
		click_delay = 0.3f;
		tv_noise = (tv_noise) ? false : true;
	}
	if (click_delay <= 0 && (glfwGetKey(renderer::get_window(), GLFW_KEY_N)))
	{
		click_delay = 0.3f;
		glitches = (glitches) ? false : true;
		if (!glitches) tv_noise = false; chroma_ab = false;
	}
	if (click_delay <= 0 && (glfwGetKey(renderer::get_window(), GLFW_KEY_O)))
	{
		click_delay = 0.3f;
		show_masks = (show_masks) ? false : true;
	}
	{
		// COUT FRAME RATE
		if (click_delay <= 0 && (glfwGetKey(renderer::get_window(), GLFW_KEY_F)))
		{
			click_delay = 0.3f;
			show_FPS = (show_FPS) ? false : true;
		}
		if (show_FPS) cout << "FPS: " << 1 / delta_time << endl;
		//press space to change cameras
		if (click_delay <= 0 && (glfwGetKey(renderer::get_window(), GLFW_KEY_SPACE))) {
			click_delay = 0.3f; //prevent SPACE SPAM
			if (current_camera == 0) //if free camera then target
				current_camera = 1;
			else //if target camera then free
				current_camera = 0;
		}
		click_delay -= delta_time;

		//rotate skybox (stars)
		skybox.get_transform().position = cam[current_camera]->get_position();
		skybox.get_transform().rotate(vec3(0.0001f, 0.0f, 0.0001f));

		//dancing point lights on the mountain

		for (int i = 4; i < 10; i++)
		{
			float R = (rand() % 10 + 1) / 10.0f;
			points[i].set_position(vec3(cos(time_delta*R) * 0.12f + points[i].get_position().x, points[i].get_position().y, sin(time_delta*R)* 0.12f + points[i].get_position().z));
			points[i].set_range(25);
		}


		//slowly rotate the moon
		meshes["moon"].get_transform().rotate(vec3(0.0003f, 0.0003f, -0.0003f));

		//spotlight circles around telescope and keeps it as it's target
		spots[0].set_position(vec3(-70, meshes["scope"].get_transform().position.y + 20, sin(time_delta) * 15 + meshes["scope"].get_transform().position.z));
		spots[0].set_direction(normalize(meshes["scope"].get_transform().position - spots[0].get_position()));
		//a mesh to track spotlight's position
		meshes["spotlight_position"].get_transform().position = spots[0].get_position() + vec3(0, 1, 0);
		meshes["rand2"].get_transform().rotate(vec3(0.1f, 0.1f, 0.1f));



		//FREE CAMERA = 0 /// TARGET CAMERA = 1
		if (current_camera == 0)
		{
			static double ratio_width = quarter_pi<float>() / static_cast<float>(renderer::get_screen_width());
			static double ratio_height = (quarter_pi<float>() * (static_cast<float>(renderer::get_screen_height()) /
				static_cast<float>(renderer::get_screen_width()))) /
				static_cast<float>(renderer::get_screen_height());

			double current_x;
			double current_y;
			// *********************************
			// Get the current cursor position
			glfwGetCursorPos(renderer::get_window(), &current_x, &current_y);
			// Calculate delta of cursor positions from last frame
			double delta_x = current_x - cursor_x;
			double delta_y = current_y - cursor_y;
			// Multiply deltas by ratios - gets actual change in orientation
			delta_x *= ratio_width;
			delta_y *= ratio_height;
			// Rotate cameras by delta
			// delta_y - x-axis rotation
			// delta_x - y-axis rotation
			static_cast<free_camera*>(cam[0])->rotate(delta_x, -delta_y);
			// Use keyboard to move the camera - WSAD
			vec3 move;

			if (glfwGetKey(renderer::get_window(), GLFW_KEY_W)) {
				move = vec3(0, 0, 40.0f) * delta_time;
			}
			if (glfwGetKey(renderer::get_window(), GLFW_KEY_S)) {
				move = vec3(0, 0, -40.0f) * delta_time;
			}
			if (glfwGetKey(renderer::get_window(), GLFW_KEY_A)) {
				move = vec3(-40.0f, 0, 0) * delta_time;
			}
			if (glfwGetKey(renderer::get_window(), GLFW_KEY_D)) {
				move = vec3(40.0f, 0, 0) * delta_time;
			}
			// Move camera
			static_cast<free_camera*>(cam[0])->move(move);
			// Update the camera
			static_cast<free_camera*>(cam[0])->update(delta_time);
			//		cout << cam[0]->get_position().x << ", " << cam[0]->get_position().y << " , " << cam[0]->get_position().z << endl;

			reflection_cam->set_up(normalize(vec3(0, 0, 1)));
			reflection_cam->set_target(vec3(2.0f*50.0f - cam[0]->get_position().x, cam[0]->get_position().y, 2.0f*20.0f - cam[0]->get_position().z));
			reflection_cam->update(delta_time);
			// Update cursor pos
			cursor_x = current_x;
			cursor_y = current_y;
			// *********************************
		}
		else
		{
			if (glfwGetKey(renderer::get_window(), GLFW_KEY_1))
			{
				cam[1]->set_position(vec3(-73.0495f, -55.9877f, 17.0259f));
				cam[1]->set_target(vec3(-0.184426f, 7.49289f, 20.6238f));
			}
			if (glfwGetKey(renderer::get_window(), GLFW_KEY_2))
			{
				cam[1]->set_position(vec3(-46.0495f, -35.9877f, 2.0259f));
				cam[1]->set_target(meshes["rand1"].get_transform().position);
			}
			if (glfwGetKey(renderer::get_window(), GLFW_KEY_3))
			{
				cam[1]->set_position(vec3(-38.0495f, -38.9877f, 13.0259f));
				cam[1]->set_target(vec3(-50.0f, -54.0f, 40.0f));
			}
			if (glfwGetKey(renderer::get_window(), GLFW_KEY_4))
			{
				cam[1]->set_position(reflection_cam->get_position());
				cam[1]->set_target(reflection_cam->get_target());
			}
			cam[1]->update(delta_time);
		}
		//ROTATE HIERARCHICAL MESHES
		for (int i = 0; i < hier_meshes.size(); i++)
			hier_meshes[i].get_transform().rotate(vec3(quarter_pi<float>(), quarter_pi<float>(), quarter_pi<float>()) * delta_time);

		//UPDATE SHADOWMAP 
		shadow.light_position = spots[0].get_position();
		shadow.light_dir = spots[0].get_direction();
		return true;
	}
}

void render_skybox()
{
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glDisable(GL_CULL_FACE);
		// Bind skybox effect
		renderer::bind(sky_eff);
		// Calculate MVP for the skybox
		auto M = skybox.get_transform().get_transform_matrix();
		auto V = cam[current_camera]->get_view();
		auto P = cam[current_camera]->get_projection();
		auto MVP = P * V * M;
		renderer::bind(cube_map, 0);
		// Set MVP matrix uniform
		glUniformMatrix4fv(sky_eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
		// Set cubemap uniform
		glUniform1i(sky_eff.get_uniform_location("cubemap"), 0);
		// Render skybox
		renderer::render(skybox);
		// Enable depth test,depth mask,face culling
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		//glCullFace(GL_FRONT);
		glEnable(GL_CULL_FACE);
}
void render_hier(camera* cam)
{
	// Bind effect
	renderer::bind(eff);
	// Get PV
	auto P = cam->get_projection();
	auto V = cam->get_view();
	auto PV = P * V;
	// Set the texture value for the shader here
	// Render meshes
	for (int i = 0; i < hier_meshes.size() ; i++) {
		// Normal matrix
		auto N = hier_meshes[i].get_transform().get_normal_matrix();
		// SET M to be the mesh transform matrix
		auto M = hier_meshes[i].get_transform().get_transform_matrix();
		// Set M matrix uniform
		glUniformMatrix4fv(eff.get_uniform_location("M"), 1, GL_FALSE, value_ptr(M));
		// Set N matrix uniform
		glUniformMatrix3fv(eff.get_uniform_location("N"), 1, GL_FALSE, value_ptr(N));
		// Apply the heirarchy chain
		for (int j = i; j > 0; j--)
		{
			M = hier_meshes[j - 1].get_transform().get_transform_matrix() * M;
		}
		// Bind material
		renderer::bind(hier_meshes[i].get_material(), "mat");
		// Set MVP matrix uniform
		glUniformMatrix4fv(eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(PV * M));
		// Bind texture to renderer
		renderer::bind(textures["rings"], 0);
		glUniform1i(eff.get_uniform_location("tex"), 0);
		renderer::bind(normal_maps["rings"], 1);
		// Set normal_map uniform
		glUniform1i(eff.get_uniform_location("normal_map"), 1);
		// Bind point lights
		renderer::bind(points, "points");
		// Bind spot lights
		renderer::bind(spots, "spots");
		// Bind texture	
		renderer::bind(light, "light");
		glUniform3fv(eff.get_uniform_location("viewPos"), 1, value_ptr(cam->get_position()));
		// Render mesh
		renderer::render(hier_meshes[i]);
	}
}

void render_liquid()
{
	renderer::bind(liquid_eff);
	// Create MVP matrix
	auto M = liquid.get_transform().get_transform_matrix();
	auto V = cam[current_camera]->get_view();
	auto P = cam[current_camera]->get_projection();
	auto MVP = P * V * M;
	// Set MVP matrix uniform


	glUniformMatrix4fv(liquid_eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
	// Set M matrix uniform
	glUniformMatrix4fv(liquid_eff.get_uniform_location("M"), 1, GL_FALSE, value_ptr(M));
	// Set N matrix uniform - remember - 3x3 matrix
	glUniformMatrix3fv(liquid_eff.get_uniform_location("N"), 1, GL_FALSE, value_ptr(liquid.get_transform().get_normal_matrix()));
	// Bind material
	renderer::bind(liquid.get_material(), "mat");	
	renderer::bind(reflection.get_frame(),0);
	glUniform1i(liquid_eff.get_uniform_location("tex"), 0);
	renderer::bind(liquid_tex, 2);
	glUniform1i(liquid_eff.get_uniform_location("tex2"), 2);
	renderer::bind(light, "light");
	// Bind normal_map
	renderer::bind(liquid_norm_map, 1);

	// Set normal_map uniform
	glUniform1i(liquid_eff.get_uniform_location("normal_map"), 1);
	// Set eye position- Get this from active camera
	glUniform3fv(liquid_eff.get_uniform_location("eye_pos"), 1, value_ptr(cam[current_camera]->get_position()));
	glUniform1f(liquid_eff.get_uniform_location("time"), time_waves);
	renderer::render(liquid);
}
void render_shadows(shadow_map s)
{

		renderer::set_render_target(s);
		// Clear depth buffer bit
		glClear(GL_DEPTH_BUFFER_BIT);
		// Set face cull mode to front
		glCullFace(GL_FRONT);

		// *********************************
		// We could just use the Camera's projection, 
		// but that has a narrower FoV than the cone of the spot light, so we would get clipping.
		// so we have yo create a new Proj Mat with a field of view of 90.
		mat4 LightProjectionMat = perspective<float>(90.f, renderer::get_screen_aspect(), 0.1f, 1000.f);

			// Bind shader
			renderer::bind(eff);
			// Render meshes
			for (auto &e : meshes) {
				auto m = e.second;
				// Create MVP matrix
				auto M = m.get_transform().get_transform_matrix();
				// *********************************
				// View matrix taken from shadow map
				auto V = s.get_view();
				// *********************************
				auto MVP = LightProjectionMat * V * M;
				// Set MVP matrix uniform
				glUniformMatrix4fv(eff.get_uniform_location("MVP"), // Location of uniform
					1,                                      // Number of values - 1 mat4
					GL_FALSE,                               // Transpose the matrix?
					value_ptr(MVP));                        // Pointer to matrix data
															// Render mesh
				renderer::render(m);
			}

		// *********************************
		// Set render target back to the screen
	renderer::set_render_target();
	// Set face cull mode to back
	glCullFace(GL_BACK);
	// *********************************

}
void render_reflection()
{
	renderer::set_render_target(reflection);
	renderer::clear();
	render_skybox();
	render_hier(reflection_cam);
	auto P = reflection_cam->get_projection();
	auto V = reflection_cam->get_view();
	auto M = meshes["mountain"].get_transform().get_transform_matrix();
	auto MVP = P * V * M;
	glUniformMatrix4fv(eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
	renderer::bind(textures["snow"], 0);
	glUniform1i(eff.get_uniform_location("tex"), 0);
	renderer::bind(meshes["mountain"].get_material(), "mat");
	renderer::render(meshes["mountain"]);
	renderer::set_render_target(temp_frame);
	renderer::clear();

}
void render_meshes()
{
	mat4 LightProjectionMat = perspective<float>(90.f, renderer::get_screen_aspect(), 0.1f, 1000.f);
	for (auto &e : meshes)
	{
		auto m = e.second;
		renderer::bind(eff);
		// Create MVP matrix
		auto M = m.get_transform().get_transform_matrix();
		auto V = cam[current_camera]->get_view();
		auto P = cam[current_camera]->get_projection();
		auto MVP = P * V * M;
		// Set MVP matrix uniform
		glUniformMatrix4fv(eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
		// Set M matrix uniform
		glUniformMatrix4fv(eff.get_uniform_location("M"), 1, GL_FALSE, value_ptr(M));
		// Set N matrix uniform - remember - 3x3 matrix
		glUniformMatrix3fv(eff.get_uniform_location("N"), 1, GL_FALSE, value_ptr(m.get_transform().get_normal_matrix()));
		// Bind material
		renderer::bind(m.get_material(), "mat");
		// Bind point lights
		renderer::bind(points, "points");
		// Bind spot lights
		renderer::bind(spots, "spots");
		// Bind texture	
		renderer::bind(light, "light");
		renderer::bind(textures[e.first], 0);
		// Set tex uniform
		glUniform1i(eff.get_uniform_location("tex"), 0);
		// Bind normal_map
		renderer::bind(normal_maps[e.first], 1);
		// Set normal_map uniform
		glUniform1i(eff.get_uniform_location("normal_map"), 1);
		// Set eye position- Get this from active camera
		glUniform3fv(eff.get_uniform_location("eye_pos"), 1, value_ptr(cam[current_camera]->get_position()));
		// Set lightMVP uniform, using:
		//Model matrix from m
		auto lM = m.get_transform().get_transform_matrix();
		// viewmatrix from the shadow map
		auto lV = shadow.get_view();
		// Multiply together with LightProjectionMat
		auto lightMVP = LightProjectionMat * lV * lM;
		// Set uniform
		glUniformMatrix4fv(eff.get_uniform_location("lightMVP"), 1, GL_FALSE, value_ptr(lightMVP));
		// Render mesh
		renderer::bind(shadow.buffer->get_depth(), 2);
		glUniform1i(eff.get_uniform_location("shadow_map"), 2);
		renderer::render(m);
	}
}
//main render, most meshes, frames , and motion blur all happen here
bool render() 
{
		//render shadows
		render_shadows(shadow);
		render_reflection();
		//render skybox
		render_skybox();
		//main meshes
		render_meshes();
		//render hier. meshes
		render_hier(cam[current_camera]);
		//liquid mesh
		render_liquid();
		// from here only rendering in screen space 
		/////
		// Set render target to current frame
		renderer::set_render_target(frames[current_frame]);
		// Clear frame
		renderer::clear();
		// Bind motion blur effect
		renderer::bind(motion_blur);
		// MVP is now the identity matrix
		auto MVP = mat4(1.0);
		// Set MVP matrix uniform
		glUniformMatrix4fv(motion_blur.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
		// Bind tempframe to TU 0.


		renderer::bind(temp_frame.get_frame(), 0);
		// Bind frames[(current_frame + 1) % 2] to TU 1.
		renderer::bind(frames[(current_frame + 1) % 2].get_frame(), 1);
		// Set tex uniforms
		glUniform1i(motion_blur.get_uniform_location("tex"), 0);
		glUniform1i(motion_blur.get_uniform_location("previous_frame"), 1);
		// Set blend factor
		glUniform1f(motion_blur.get_uniform_location("blend_factor"), blend_factor);
		// Render screen quad
		renderer::render(screen_quad);
		// Set render target back to the screen
		renderer::set_render_target();
		renderer::clear();
		renderer::bind(tex_eff);
		// Set MVP matrix uniform
		glUniformMatrix4fv(tex_eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
		// Bind texture from frame buffer
		renderer::bind(frames[current_frame].get_frame(), 3);
		// Set the uniform
		glUniform1i(tex_eff.get_uniform_location("tex"), 3);
		renderer::bind(textures["mask"], 4);
		glUniform1i(tex_eff.get_uniform_location("mask"), 4);
		renderer::bind(textures["camera"], 5);
		glUniform1i(tex_eff.get_uniform_location("camera"), 5);
		glUniform1i(tex_eff.get_uniform_location("help"), help);
		glUniform1i(tex_eff.get_uniform_location("fisheye"), fisheye);
		glUniform1i(tex_eff.get_uniform_location("grayscale"), grayscale);
		glUniform1i(tex_eff.get_uniform_location("LSD"), LSD);		
		glUniform1i(tex_eff.get_uniform_location("chroma_ab"), chroma_ab);
		glUniform1f(tex_eff.get_uniform_location("time"), time_delta);
		glUniform1i(tex_eff.get_uniform_location("tv_noise"), tv_noise);
		glUniform1i(tex_eff.get_uniform_location("show_masks"), show_masks);
		// Render the screen quad
		renderer::render(screen_quad);
		// *********************************
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
  //free up memory to prevent leaks
  delete cam[0];
  delete cam[1];
  delete reflection_cam;





}