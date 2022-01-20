#pragma once
#ifndef __MLTRENDERER_HPP__
#define __MLTRENDERER_HPP__

#include "scene/Scene.hpp"
#include "intersections/HitRecord.hpp"
#include "shaders/ShaderCreator.hpp"

#include "Camera.hpp"
#include "Ray.hpp"
#include "MLT.hpp"

#include <tuple>

namespace MetropolisLightTransport
{
    using namespace NRenderer;
    using namespace std;
    
    struct PathSample{
        int x;
        int y;
        RGB color;
        float weight;
        PathSample(const int x_=0, const int y_=0, const RGB &color_=RGB(), const float weight_=1.0f):
        x(x_), y(y_), color(color_), weight(weight_) {}
    };

    class MLTRenderer
    {
    public:
    private:
        SharedScene spScene;
        Scene& scene;

        unsigned int width;
        unsigned int height;
        unsigned int depth;
        unsigned int samples;

        MetropolisLightTransport::Camera camera;

        vector<SharedShader> shaderPrograms;
    public:
        MLTRenderer(SharedScene spScene)
            : spScene                (spScene)
            , scene                  (*spScene)
            , camera                 (spScene->camera)
        {
            width = scene.renderOption.width;
            height = scene.renderOption.height;
            depth = scene.renderOption.depth;
            samples = scene.renderOption.samplesPerPixel;
        }
        ~MLTRenderer() = default;

        using RenderResult = tuple<RGBA*, unsigned int, unsigned int>;
        RenderResult render();
        void release(const RenderResult& r);
    private:
        PathSample generate_new_path(MLT& mlt);
        RGB gamma(const RGB& rgb);
        RGB trace(const Ray& ray, int currDepth);
        HitRecord closestHitObject(const Ray& r);
        tuple<float, Vec3> closestHitLight(const Ray& r);
    };
}

#endif