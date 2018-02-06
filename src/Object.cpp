#include "stdafx.h"
#include "Object.h"

Object::Object()
{
	ResetToWorld();
}

Object::~Object()
{
	// Properly de-allocate all resources once they've outlived their purpose.
	glDeleteVertexArrays(1, &m_VAO);
	glDeleteBuffers(1, &m_VBO);
	glDeleteBuffers(1, &m_EBO);
}

unsigned char* Object::LoadPPM(const char* filename, int& width, int& height)
{
	const int BUFSIZE = 128;
	FILE* fp;
	unsigned int read;
	unsigned char* rawData;
	char buf[3][BUFSIZE];
	char* retval_fgets;
	size_t retval_sscanf;
	//Read in the ppm file.
	if ((fp = fopen(filename, "rb")) == NULL)
	{
		std::cerr << "error reading ppm file, could not locate " << filename << std::endl;
		width = 0;
		height = 0;
		return NULL;
	}
	//Read magic number:
	retval_fgets = fgets(buf[0], BUFSIZE, fp);
	//Read width and height:
	do
	{
		retval_fgets = fgets(buf[0], BUFSIZE, fp);
	} while (buf[0][0] == '#');
	retval_sscanf = sscanf(buf[0], "%s %s", buf[1], buf[2]);
	width = atoi(buf[1]);
	height = atoi(buf[2]);
	//Read maxval:
	do
	{
		retval_fgets = fgets(buf[0], BUFSIZE, fp);
	} while (buf[0][0] == '#');
	//Read image data:
	rawData = new unsigned char[width * height * 3];
	read = (unsigned int)fread(rawData, width * height * 3, 1, fp);
	fclose(fp);
	if (read != 1)
	{
		std::cerr << "error parsing ppm file, incomplete data" << std::endl;
		delete[] rawData;
		width = 0;
		height = 0;
		return NULL;
	}
	return rawData; // Return rawData or 0 if failed.
}

GLuint Object::LoadCubemap(std::vector<const char*> faces)
{
	// Hold the textureID (This will be the textureID to return).
	GLuint textureID;

	// Define variables to hold height map's width, height, pixel information.
	int width, height;
	unsigned char * image;

	// Create ID for texture.
	glGenTextures(1, &textureID);
	glActiveTexture(GL_TEXTURE0);// Set this texture to be the active texture (0).

	// Set this texture to be the one we are working with.
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	// Generate the texture.
	for (GLuint i = 0; i < faces.size(); i++)
	{
		std::string fileName = std::string(faces[i]);

		// We search for the file extension.
		auto ext = fileName.find_last_of('.');
		if (ext == std::string::npos) continue;

		// If there's the extension, we attempt to open it by type.
		// TODO: put in other image types.
		// For now, we'll support just ppm. This will make it more flexible to load cubemaps, and possibly mix/match file types.
		std::string extension = fileName.substr(ext + 1);
		if (extension.compare("ppm") == 0) image = Object::LoadPPM(fileName.c_str(), width, height);
		else image = 0;

		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	}

	// Make sure no bytes are padded:
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Select GL_MODULATE to mix texture with polygon color for shading:
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	// Use bilinear interpolation:
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// Use clamp to edge to hide skybox edges:
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);//X
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);//Y
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);//Z

	// Unbind the texture cube map.
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	// Return the textureID, we need to keep track of this texture variable.
	return textureID;
}

int Object::LoadData(const std::vector<glm::vec3> &vertices, const std::vector<glm::vec3> &normals, const std::vector<glm::vec2> &texCoords, const std::vector<unsigned int> &indices, const int &count)
{
	// Let us enforce that count is always less than or equal to the number of elements.
	int vertices_size = (int)vertices.size();
	int normals_size = (int)normals.size();
	int texCoords_size = (int)texCoords.size();

	// Data
	for (int i = 0; i < count; i++)
	{
		S_Container container;
		if (i < vertices_size) container.m_Vertex = vertices[i];
		if (i < normals_size) container.m_Normal = normals[i];
		if (i < texCoords_size) container.m_TexCoord = texCoords[i];
		m_Data.push_back(container);
	}

	// Indices
	m_Indices = indices;

	return 0;
}

int Object::LoadDataFromFile(const char * file)
{
	// Initialize min, max, scale values for each coordinate.
	float min_X = INFINITY, min_Y = INFINITY, min_Z = INFINITY; // Minimum set to infinity so first value is always inputted.
	float max_X = -INFINITY, max_Y = -INFINITY, max_Z = -INFINITY; // Maximum set to -infinity so first value is always inputted.

	// Same for scale, used to find "max" scale or the longest axis. 
	// This ensures the ranges of vertices are [-1, 1].
	float avg_X = 0.0f, avg_Y = 0.0f, avg_Z = 0.0f, scale_v = -INFINITY;

	// Open the file for reading called objFile.
	std::FILE * objFile = fopen(file, "r");

	// If the file is unable to open return -1: fail.
	if (objFile == NULL) {
		printf("Unable to read file: %s\n", file);
		return -1;
	}

	std::vector<glm::vec3> vertices, normals;
	std::vector<glm::vec2> texCoords;
	std::vector<unsigned int> indices;

	// Read the file until the end. "# are commments to be ignored".
	while (1) {
		char buf[BUFSIZ];
		int check = fscanf(objFile, "%s", buf);
		if (check == EOF) break;

		// Read in lines that start with "v". Add into vertices.
		if (strcmp(buf, "v") == 0) {
			glm::vec3 vertex;
			fscanf(objFile, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
			vertices.push_back(vertex);

			// Calculate min, max for x, y, z.
			if (vertex.x < min_X) { min_X = vertex.x; }
			if (vertex.y < min_Y) { min_Y = vertex.y; }
			if (vertex.z < min_Z) { min_Z = vertex.z; }
			if (vertex.x > max_X) { max_X = vertex.x; }
			if (vertex.y > max_Y) { max_Y = vertex.y; }
			if (vertex.z > max_Z) { max_Z = vertex.z; }
			continue;
		}

		// Read in lines that start with "vn". Add into normals.
		if (strcmp(buf, "vn") == 0) {
			glm::vec3 normal;
			fscanf(objFile, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
			normals.push_back(normal);
			continue;
		}

		// Read in lines that start with "vt". Add into textures.
		if (strcmp(buf, "vt") == 0) {
			glm::vec2 texCoord;
			fscanf(objFile, "%f %f\n", &texCoord.x, &texCoord.y);
			texCoords.push_back(texCoord);
			continue;
		}

		// Read in lines that start with "f". Add into indices.
		if (strcmp(buf, "f") == 0) {//Note: Read only left or right side since they are identical on both sides.
			unsigned int faces_v[3], faces_vn[3];
			fscanf(objFile, "%d//%d %d//%d %d//%d\n", &faces_v[0], &faces_vn[0], &faces_v[1], &faces_vn[1], &faces_v[2], &faces_vn[2]);
			//unsigned int faces_v[3], faces_vn[3], faces_vt[3];
			//fscanf(objFile, "%d/%d/%d %d/%d/%d %d/%d/%d\n", &faces_v[0], &faces_vt[0], &faces_vn[0], &faces_v[1], &faces_vt[1], &faces_vn[1], &faces_v[2], &faces_vt[2], &faces_vn[2]);
			indices.push_back(faces_v[0] - 1);
			indices.push_back(faces_v[1] - 1);
			indices.push_back(faces_v[2] - 1);
			continue;
		}
	}
	fclose(objFile);

	// Calculate average x, y, z.
	avg_X = (min_X + max_X) / 2;
	avg_Y = (min_Y + max_Y) / 2;
	avg_Z = (min_X + max_Z) / 2;

	// Calculate scale value.
	float scale_x = (max_X - min_X);
	float scale_y = (max_Y - min_Y);
	float scale_z = (max_Z - min_Z);
	if (scale_x > scale_v) { scale_v = scale_x; }
	if (scale_y > scale_v) { scale_v = scale_y; }
	if (scale_z > scale_v) { scale_v = scale_z; }
	
	// Let us enforce that count is always less than or equal to the number of elements.
	int vertices_size = (int)vertices.size();
	int normals_size = (int)normals.size();
	int texCoords_size = (int)texCoords.size();

	// Subtract the average to center all objects and multiply by (1/scale) to bring them down to size.
	for (int i = 0; i < vertices.size(); i++)
	{
		vertices[i].x = vertices[i].x - avg_X;
		vertices[i].y = vertices[i].y - avg_Y;
		vertices[i].z = vertices[i].z - avg_Z;
		vertices[i] *= (1.0f / (scale_v));

		// Throw everything into a container to hold all values.
		S_Container container;
		if (i < vertices_size) container.m_Vertex = vertices[i];
		if (i < normals_size) container.m_Normal = normals[i];
		if (i < texCoords_size) container.m_TexCoord = texCoords[i]; // Leaving this empty for now to load simple objs.
		m_Data.push_back(container);
	}

	// Update indices.
	m_Indices = indices;

	// Return 0 if successful.
	return 0;
}

void Object::LoadDataIntoBuffers()
{
	// Make sure we actually have data first. This checks if data was loaded yet.
	if ((int)m_Data.size() < 0) return;

	// Create buffers/arrays.
	glGenVertexArrays(1, &m_VAO);
	glGenBuffers(1, &m_VBO);
	glGenBuffers(1, &m_EBO);

	// Bind the Vertex Array Object first, then bind and set vertex buffer(s) and attribute pointer(s).
	glBindVertexArray(m_VAO); //Bind vertex array object.

	glBindBuffer(GL_ARRAY_BUFFER, m_VBO); //Bind Container buffer.
	glBufferData(GL_ARRAY_BUFFER, m_Data.size() * sizeof(S_Container), &m_Data[0], GL_STATIC_DRAW); // Set vertex buffer to the Container.

	// Vertex Positions.
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(S_Container), (GLvoid*)offsetof(S_Container, m_Vertex));

	// Vertex Normals.
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(S_Container), (GLvoid*)offsetof(S_Container, m_Normal));

	// Vertex Texture Coords.
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(S_Container), (GLvoid*)offsetof(S_Container, m_TexCoord));

	// If there are indices, we can set them here, otherwise we don't use them.
	if ((int)m_Indices.size() > 0)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO); // Bind indices buffer.
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_Indices.size() * sizeof(int), &m_Indices[0], GL_STATIC_DRAW);
	}

	// Unbind.
	glBindBuffer(GL_ARRAY_BUFFER, 0); // Note that this is allowed, the call to glVertexAttribPointer registered VBO as the currently bound vertex buffer object so afterwards we can safely unbind.
	glBindVertexArray(0); // Unbind VAO (it's always a good thing to unbind any buffer/array to prevent strange bugs), remember: do NOT unbind the EBO, keep it bound to this VAO.

	// Data is loaded, buffers are populated, so we can render now.
	m_IsInit = true;
}

void Object::Translate(const glm::vec3 &value)
{
	m_ToWorld = glm::translate(glm::mat4(1.0f), value) * m_ToWorld;
}

void Object::Rotate(const float degree, const glm::vec3 &axis)
{
	m_ToWorld = glm::rotate(glm::mat4(1.0f), (degree / 180.0f * glm::pi<float>()), axis) * m_ToWorld;
}

void Object::Scale(const glm::vec3 &value)
{
	m_ToWorld = glm::scale(glm::mat4(1.0f), value) * m_ToWorld;
}

void Object::SetPosition(const glm::vec3 & value)
{
	m_ToWorld[3] = glm::vec4(value, 1.0f);
}
