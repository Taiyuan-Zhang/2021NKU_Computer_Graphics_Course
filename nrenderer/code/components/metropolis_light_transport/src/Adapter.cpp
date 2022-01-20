#include "component/RenderComponent.hpp"
#include "server/Server.hpp"
#include "scene/Scene.hpp"
#include "Camera.hpp"

#include "MLTRenderer.hpp"

using namespace std;
using namespace NRenderer;

namespace MetropolisLightTransport
{
    // 继承RenderComponent, 复写render接口
    class Adapter : public RenderComponent
    {
        void render(SharedScene spScene) {
            MLTRenderer renderer{spScene};
            auto renderResult = renderer.render();
            auto [pixels, width, height] = renderResult;
            getServer().screen.set(pixels, width, height);
            renderer.release(renderResult);
        }
    };
}

// REGISTER_RENDERER(Name, Description, Class)
const static string description =
    "Metropolis Light Transport.\n"
    "Supported:\n" 
    " - TODO"
    ;

REGISTER_RENDERER(MetropolisLightTransport, description, MetropolisLightTransport::Adapter);