#ifndef ResourcesManager_h
#define ResourcesManager_h

#include <unordered_map>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include <d3d11.h>

#include "../resources/meshes.h"

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

class ResourcesManager {

public:
	
	static std::string getStringForShader(const std::string & shaderName);
	
	static void loadResources(ID3D11Device* device);
	
	static ID3D11ShaderResourceView* getTextureFor(const std::string & fileName);
	
	static glm::vec2 getTextureSizeFor(const std::string & fileName);
	
private:
	
	static  unsigned char* getDataForImage(const std::string & fileName, unsigned int & imwidth, unsigned int & imheight);
	
	static std::unordered_map<std::string, std::string> shadersLibrary;
	
	static std::unordered_map<std::string, unsigned char*> imagesLibrary;
	
	static std::unordered_map<std::string, glm::vec2> imagesSize;
	
	static std::unordered_map<
		std::string,
		ComPtr<ID3D11ShaderResourceView>
	> textureLibrary;
};

#endif
