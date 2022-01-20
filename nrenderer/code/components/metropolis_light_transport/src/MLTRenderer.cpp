#include "server/Server.hpp"

#include "MLTRenderer.hpp"

#include "VertexTransformer.hpp"
#include "intersections/intersections.hpp"

#include "glm/gtc/matrix_transform.hpp"
#include "MLT.hpp"
#include <stack>
#include <algorithm>

namespace MetropolisLightTransport
{
    RGB MLTRenderer::gamma(const RGB& rgb){
        return glm::sqrt(rgb);
    }

    PathSample MLTRenderer::generate_new_path(MLT& mlt){
        int x, y;
        float weight = 4.0f;
        weight *= width;
        x = mlt.NextSample()*width;
        if(x == width)
            x = 0;
        weight *= height;
        y = mlt.NextSample()*height;
        if(y == height)
            y = 0;
        auto r = defaultSamplerInstance<UniformInSquare>().sample2d();
        float rx = r.x;
        float ry = r.y;
        float sx = rx/float(width);
        float sy = ry/float(height);
        auto ray = camera.shoot(sx, sy);
        
        RGB color = trace(ray, 0);
        cout <<"Generate " << x << " " << y << endl;
        return PathSample(x, y, color, 1.0f/(1.0f/weight));
    }

    auto MLTRenderer::render() -> RenderResult{
        // shaders
        shaderPrograms.clear();
        ShaderCreator shaderCreator{};
        for (auto& m : scene.materials) {
            shaderPrograms.push_back(shaderCreator.create(m, scene.textures));
        }

        RGBA* pixels = new RGBA[width*height]{};
        RGB* tmp = new RGB[width*height]{};

        // 局部坐标转换成世界坐标
        VertexTransformer vertexTransformer{};
        vertexTransformer.exec(spScene);

        MLT mlt;

        const float b;
        const float p_large = 0.5f;
        const int mutation = samples*width*height;
        int accept = 0, reject = 0;
        PathSample old_path;

        mlt.large_step = 1;

        while(1){
            mlt.InitUsedRandCoords();
            PathSample sample = generate_new_path(mlt);
            mlt.global_time++;

            while(!mlt.primary_samples_stack.empty())
                mlt.primary_samples_stack.pop();
            auto value = luminance(sample.color);
            if(value > 0.0f){
                b = value;
                old_path = sample;
                break;
            }
        }

        for(int i=0; i<mutation; i++){
            mlt.large_step = rand01()<p_large;
            mlt.InitUsedRandCoords();

            PathSample new_path = generate_new_path(mlt);
            float a = std::min(1.0f, luminance(new_path.color)/luminance(old_path.color));

            const float new_path_weight = (a+mlt.large_step)/(luminance(new_path.color)/b+p_large)/mutation;
            const float old_path_weight = (1.0f-a)/(luminance(old_path.color)/b+p_large)/mutation;

            tmp[new_path.y*width+new_path.x] = tmp[new_path.y*width+new_path.x] + new_path.weight*new_path_weight*new_path.color;
            tmp[old_path.y*width+old_path.x] = tmp[old_path.y*width+old_path.x] + old_path.weight*old_path_weight*old_path.color;

            if(rand01() < a){
                accept++;
                old_path = new_path;
                if(mlt.large_step)
                    mlt.large_step_time = mlt.global_time;
                mlt.global_time++;
                while(!mlt.primary_samples_stack.empty())
                    mlt.primary_samples_stack.pop();
            }
            else{
                reject++;
                int index = mlt.used_rand_coords-1;
                while(!mlt.primary_samples_stack.empty()){
                    mlt.primary_samples[index--] = mlt.primary_samples_stack.top();
                    mlt.primary_samples_stack.pop();
                }
            }
        }
        

        for(int i=0; i<height*width; i++){
            pixels[i] = {glm::clamp(tmp[i], 0.0f, 1.0f), 1};
        }

        getServer().logger.log("Done...");
        return {pixels, width, height};
    }

    void MLTRenderer::release(const RenderResult& r){
        auto [p, w, h] = r;
        delete[] p;
    }

    HitRecord MLTRenderer::closestHitObject(const Ray& r) {
        HitRecord closestHit = nullopt;
        float closest = FLOAT_INF;
        for (auto& s : scene.sphereBuffer) {
            auto hitRecord = Intersection::xSphere(r, s, 0.000001, closest);
            if (hitRecord && hitRecord->t < closest) {
                closest = hitRecord->t;
                closestHit = hitRecord;
            }
        }
        for (auto& t : scene.triangleBuffer) {
            auto hitRecord = Intersection::xTriangle(r, t, 0.000001, closest);
            if (hitRecord && hitRecord->t < closest) {
                closest = hitRecord->t;
                closestHit = hitRecord;
            }
        }
        for (auto& p : scene.planeBuffer) {
            auto hitRecord = Intersection::xPlane(r, p, 0.000001, closest);
            if (hitRecord && hitRecord->t < closest) {
                closest = hitRecord->t;
                closestHit = hitRecord;
            }
        }
        return closestHit; 
    }
    tuple<float, Vec3> MLTRenderer::closestHitLight(const Ray& r) {
        Vec3 v = {};
        HitRecord closest = getHitRecord(FLOAT_INF, {}, {}, {});
        for (auto& a : scene.areaLightBuffer) {
            auto hitRecord = Intersection::xAreaLight(r, a, 0.000001, closest->t);
            if (hitRecord && closest->t > hitRecord->t) {
                closest = hitRecord;
                v = a.radiance;
            }
        }
        return { closest->t, v };
    }

    RGB MLTRenderer::trace(const Ray& r, int currDepth) {
        if (currDepth == depth) return scene.ambient.constant;
        auto hitObject = closestHitObject(r);
        auto [ t, emitted ] = closestHitLight(r);
        // hit object
        if (hitObject && hitObject->t < t) {
            auto mtlHandle = hitObject->material;
            auto scattered = shaderPrograms[mtlHandle.index()]->shade(r, hitObject->hitPoint, hitObject->normal);
            auto scatteredRay = scattered.ray;
            auto attenuation = scattered.attenuation;
            auto emitted = scattered.emitted;
            auto next = trace(scatteredRay, currDepth+1);
            float n_dot_in = glm::dot(hitObject->normal, scatteredRay.direction);
            float pdf = scattered.pdf;
            /**
             * emitted      - Le(p, w_0)
             * next         - Li(p, w_i)
             * n_dot_in     - cos<n, w_i>
             * atteunation  - BRDF
             * pdf          - p(w)
             **/
            return emitted + attenuation * next * n_dot_in / pdf;
        }
        // 
        else if (t != FLOAT_INF) {
            return emitted;
        }
        else {
            return Vec3{0};
        }
    }
}