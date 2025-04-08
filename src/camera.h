#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
struct CameraSettings
{
    float aspect_ratio = 1.0f;
    int image_width = 100;
    int samples_per_pixel = 10;
    int max_depth = 10;
    float vfov = 90;
    float focus_dist = 10.0;
    float defocus_angle = 0.0;
    glm::vec3 lookfrom = glm::vec3(0, 0, 0);
    glm::vec3 lookat = glm::vec3(0, 0, -1);
    glm::vec3 vup = glm::vec3(0, 1, 0);
};

// Struct used for sending to gpu
struct CameraData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 inv_view;
    glm::mat4 inv_projection;
    glm::vec4 lookfrom; // camera position, std140 aligns vec3 to vec4
    float focus_distance;
    float defocus_angle;
};

class Camera {
    public:
        CameraSettings settings;
        CameraData data;

        int image_width;
        int image_height;
    
        Camera(CameraSettings &settings) {
            data.lookfrom = glm::vec4(settings.lookfrom, 1.0f);
            image_width = settings.image_width;
            image_height = (int)(settings.image_width / settings.aspect_ratio);

            data.view = glm::lookAt(settings.lookfrom, settings.lookat, settings.vup);
            data.projection = glm::perspective(glm::radians(settings.vfov), settings.aspect_ratio, 0.1f, 1000.0f);
    
            data.inv_view = glm::inverse(data.view);
            data.inv_projection = glm::inverse(data.projection);
            
            data.focus_distance = settings.focus_dist;
            data.defocus_angle = settings.defocus_angle;
        }

    };
    