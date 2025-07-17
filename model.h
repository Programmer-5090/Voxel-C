#pragma once

#include <stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "mesh.h"
#include <iostream>


class Model 
{
    public:
        // model data
        std::vector<Mesh> meshes;
        
        Model(const char *path, bool gamma = false): gammaCorrection(gamma)
        {
            loadModel(path);
        }
        void Draw(Shader &shader);

        void DrawMesh(Shader &shader, unsigned int index);

    private:
        std::string directory;
        std::vector<Texture> textures_loaded;
        bool gammaCorrection;

        void loadModel(std::string path);
        void processNode(aiNode *node, const aiScene *scene);
        Mesh processMesh(aiMesh *mesh, const aiScene *scene);
        std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, 
                                             std::string typeName);
};