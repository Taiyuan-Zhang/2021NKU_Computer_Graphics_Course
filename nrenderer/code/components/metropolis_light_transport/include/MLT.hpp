#pragma once
#ifndef __MLT_HPP__
#define __MLT_HPP__

#include <stack>
#include "glm/gtc/matrix_transform.hpp"

namespace MetropolisLightTransport
{
    using namespace NRenderer;
    using namespace std;

    inline unsigned int xor128(void){
        static unsigned int x=123456789;
        static unsigned int y=362436069;
        static unsigned int z=521288629;
        static unsigned int w=88675123;
        unsigned int t;
        
        t = x^(x<<11);
        x=y; y=z; z=w;
        return w=(w^(w>>19))^(t^(t>>8));
    }
    inline float rand01(){
        return static_cast<float>((double)xor128()/(UINT_MAX));
    }

    struct PrimarySample{
        int modify_time;
        float value;
        PrimarySample(){
            modify_time = 0;
            value = rand01();
        }
    };

    class MLT{
    public:
        int global_time;
        int large_step;
        int large_step_time;
        int used_rand_coords;

        vector<PrimarySample> primary_samples;
        stack<PrimarySample> primary_samples_stack;

        MLT() {
            global_time = large_step = large_step_time = used_rand_coords = 0;
            primary_samples.resize(128);
        }
        void InitUsedRandCoords(){
            used_rand_coords = 0;
        }

        float NextSample(){
            if(primary_samples.size()<=used_rand_coords){
                primary_samples.resize(primary_samples.size()*1.5);
            }
            if(primary_samples[used_rand_coords].modify_time<global_time){
                if(large_step>0){
                    primary_samples_stack.push(primary_samples[used_rand_coords]);
                    primary_samples[used_rand_coords].modify_time = global_time;
                    primary_samples[used_rand_coords].value = rand01();
                }
                else{
                    if(primary_samples[used_rand_coords].modify_time<large_step_time){
                        primary_samples[used_rand_coords].modify_time = large_step_time;
                        primary_samples[used_rand_coords].value = rand01();
                    }
                    while(primary_samples[used_rand_coords].modify_time<global_time-1){
                        primary_samples[used_rand_coords].value = Mutate(primary_samples[used_rand_coords].value);
                        primary_samples[used_rand_coords].modify_time++;
                    }
                    primary_samples_stack.push(primary_samples[used_rand_coords]);
                    primary_samples[used_rand_coords].value = Mutate(primary_samples[used_rand_coords].value);
                    primary_samples[used_rand_coords].modify_time = global_time;
                }

                used_rand_coords++;
                return primary_samples[used_rand_coords-1].value;
            }
        }
    private:
        float Mutate(const float x){
            const float r = rand01();
            const float s1 = 1.0f/512.0f;
            const float s2 = 1.0f/16.0f;
            const float dx = s1/(s1/s2+fabsf(2.0f*r-1.0f))-s1/(s1/s2+1.0f);
            if(r<0.5f){
                const float x1 = x+dx;
                return (x1<1.0f)?x1:x1-1.0f;
            }
            else{
                const float x1 = x-dx;
                return (x1<0.0f)?x1+1.0f:x1;
            }
        }
    };

    inline float luminance(const RGB& color){
        return glm::dot(glm::vec3(0.2126, 0.7152, 0.0722), color);
    }
}

#endif