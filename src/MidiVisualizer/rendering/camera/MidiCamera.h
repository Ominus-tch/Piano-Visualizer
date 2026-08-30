#ifndef MidiCamera_h
#define MidiCamera_h

#include <glm/glm.hpp>

class MidiCamera {
	
public:
	
	MidiCamera();

	~MidiCamera();
	
	/// Reset the position of the camera.
	void reset();
	
	/// Update the screen size and projection matrix.
	void screen(int width, int height, float scale);

	const glm::mat4 & view() const { return _view; };

	const glm::mat4 & projection() const { return _projection; };

	const glm::ivec2 & screenSize() const { return _screenSize; };

	const glm::ivec2 & renderSize() const { return _renderSize; };

	const float & scale() const { return _scale; };
	
private:

	/// The view matrix.
	glm::mat4 _view;
	/// The projection matrix.
	glm::mat4 _projection;
	// Screen size
	glm::ivec2 _screenSize;
	// Size use for render targets.
	glm::ivec2 _renderSize;
	
	/// Vectors defining the view frame.
	glm::vec3 _eye;
	glm::vec3 _center;
	glm::vec3 _up;
	glm::vec3 _right;

	float _scale;
	

};

#endif
