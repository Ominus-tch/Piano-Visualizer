#include <stdio.h>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

#include "MidiCamera.h"


MidiCamera::MidiCamera() {
	reset();
}

MidiCamera::~MidiCamera(){}

void MidiCamera::reset(){
	_eye = glm::vec3(0.0f, 0.0f, 1.0f);
	_center = glm::vec3(0.0f, 0.0f, 0.0f);
	_up = glm::vec3(0.0f, 1.0f, 0.0f);

	_view = glm::lookAt(_eye, _center, _up);

	_scale = 1.0f;
}

void MidiCamera::screen(int width, int height, float scaling){
	_screenSize[0] = (width > 0 ? width : 1);
	_screenSize[1] = (height > 0 ? height : 1);
	_scale = scaling;

	_renderSize = glm::ivec2(glm::round(glm::vec2(_screenSize) / _scale));
	// Perspective projection.
	_projection = glm::perspective(45.0f, float(_renderSize[0]) / float(_renderSize[1]), 0.1f, 100.f);
}




