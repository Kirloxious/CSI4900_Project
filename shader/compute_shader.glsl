#version 460 core

#define MAX_NUM_SPHERES 10
//Max local_work group sizes: x = 1024, y = 1024, z = 64
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(rgba32f, location = 0)  readonly uniform image2D srcImage;
layout(rgba32f, location = 1) writeonly uniform image2D destImage;
layout(location = 2) uniform vec3 cameraPosition;
layout(location = 3) uniform int time;
layout(location = 4) uniform int frameIndex;

struct Camera {
    vec3  center;         // Camera center
    vec3  pixel00_loc;    // Location of pixel 0, 0
    vec3  pixel_delta_u;  // Offset to pixel to the right
    vec3  pixel_delta_v;  // Offset to pixel below
    vec3  u, v, w;         // Camera frame basis vectors
    vec3  defocus_disk_u;    // Defocus disk horizontal radius
    vec3  defocus_disk_v;    // Defocus disk vertical radius
    double defocus_angle; 
};


struct Ray {
    vec3 origin;
    vec3 direction;
};

struct Sphere{
    vec3 position;
    float radius;
    uint material_index;
};
// Materials indecies
// While creating the spheres, we pass in the material index to let the scatter function know how to scatter the ray
// Diffuse/Lambertian: 0
// Metal: 1
// Dielectric: 2

struct Material{
    vec4 color;
    // int reflecting;
};


// variables

uniform int num_objects;

layout(binding = 0) uniform SpheresBuffer{
    Sphere spheres[MAX_NUM_SPHERES];
};

layout(std140, binding = 1) uniform MatsBuffer{
    Material mats[MAX_NUM_SPHERES];
};

// layout(std140) uniform Mats_indicies{
//     int indicies[MAX_NUM_SPHERES];
// };

const float infinity = 1./0.;

// The state must be initialized to non-zero value
uint XOrShift32(inout uint state)
{
    // https://en.wikipedia.org/wiki/Xorshift
    uint x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

float RandomUnilateral(inout uint state) {
    return float(XOrShift32(state)) / float(uint(4294967295));
}

float RandomBilateral(inout uint state) {
    return 2.0f * RandomUnilateral(state) - 1.0f;
}

vec3 random_vec3(uint state) {
    return vec3(RandomUnilateral(state), RandomUnilateral(state), RandomUnilateral(state));
}


vec3 random_unit_vector(uint state) {
    vec3 p;
    float lensq;
    do {
        p = random_vec3(state);
        lensq = dot(p, p);
    } while (lensq <= 1e-160 || lensq > 1.0);
    
    return p / sqrt(lensq);
}

vec3 random_on_hemisphere(uint state, vec3 normal) {
    vec3 direction = random_unit_vector(state);
    if (dot(direction, normal) > 0.0) { // same hemisphere as normal
        return direction;
    } else {
        return -direction;
    }
}




struct HitRecord {
    vec3 point;
    vec3 normal;
    uint mat_index;
    float t;
    bool front_face;
};



/** Hit record functions **/

bool set_face_normal(in Ray ray, in vec3 outward_normal, inout HitRecord hit_rec) {
    hit_rec.front_face = dot(ray.direction, outward_normal) < 0.0;
    hit_rec.normal = hit_rec.front_face ? outward_normal : -outward_normal;
    return true;
}

/** End Hit record **/

/** Intersection functions **/

bool hit_sphere(in Ray r, in Sphere s, float ray_tmin, float ray_tmax, inout HitRecord hit_rec) {
    vec3 oc = s.position - r.origin; 
    float a = dot(r.direction, r.direction);
    float h = dot(oc, r.direction);
    float c = dot(oc, oc) - s.radius * s.radius; 
    
    float discriminant = h * h - a * c;
    if (discriminant < 0.0) 
        return false;

    float sqrtd = sqrt(discriminant);
    float root = (h - sqrtd) / a;
    if (root < ray_tmin || root > ray_tmax){
        root = (h + sqrtd) / a;
        if (root < ray_tmin || root > ray_tmax)
            return false;
    }
    
    hit_rec.t = root;
    hit_rec.point = r.origin + hit_rec.t * r.direction ;
    hit_rec.normal = normalize(hit_rec.point - s.position);
    hit_rec.mat_index = s.material_index;
    set_face_normal(r, hit_rec.normal, hit_rec);

    return true;
}

/** End Intersections **/


/** Ray functions **/
bool world_hit(in Ray r, in float ray_tmin,  in float ray_tmax, inout HitRecord hit_rec) {
    HitRecord temp_rec;
    bool hit_anything = false;
    float closest_so_far = ray_tmax;
    for (int i = 0; i < num_objects; i++) {
        Sphere s = spheres[i];
        if (hit_sphere(r, s, ray_tmin, closest_so_far, temp_rec)) {
            if (temp_rec.t < closest_so_far){
                hit_anything = true;
                closest_so_far = temp_rec.t;
                hit_rec = temp_rec;
            }
        }
    }
    return hit_anything;
}

vec3 lambertian_shading(vec3 point, vec3 normal, vec3 light_pos) {
    vec3 light_dir = normalize(light_pos - point);
    float intensity = max(dot(normal, light_dir), 0.0);
    return vec3(1.0) * intensity;  // Assuming white light
}

bool scatter(uint state, in Ray ray_in, in HitRecord hit_rec, inout vec3 attenuation, inout Ray scattered) {
    vec3 refracted;
    if (mats[hit_rec.mat_index].color.a == 1.0) {
        refracted = reflect(ray_in.direction, hit_rec.normal);
    } else {
        refracted = hit_rec.normal + random_unit_vector(state);   
        if(length(refracted) < 0.0001) 
            refracted = hit_rec.normal; 
    }

    refracted = normalize(refracted);
    scattered = Ray(hit_rec.point, refracted);
    attenuation = mats[hit_rec.mat_index].color.rgb;
    return true;
}

vec3 ray_color(in Ray ray, uint max_bounces, inout uint state) {
    vec3 accumulated_color  = vec3(1.0);
    
    Ray current_ray;
    current_ray.origin = ray.origin;
    current_ray.direction = ray.direction;

    uint bounces_computed = 0;
    for (int i = 0; i < max_bounces; i++) {
        HitRecord hit_rec;
        if (world_hit(current_ray, 0.001, infinity, hit_rec)) {
            Material mat = mats[hit_rec.mat_index];
            ++bounces_computed;

            Ray scattered;
            vec3 attenuation;
            if (scatter(state, current_ray, hit_rec, attenuation, scattered)) {
                accumulated_color *= attenuation;
                current_ray = scattered;
            }
            
        } else {
            vec3 unit_direction = normalize(current_ray.direction);
            float blend = 0.5 * (unit_direction.y + 1.0);
            return accumulated_color * mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), blend);
        }
    }
    return vec3(0.0); // black if max depth reached
}

vec3 sample_square(uint state) {
    // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit square.
    return vec3(RandomBilateral(state) - 0.5, RandomBilateral(state) - 0.5, 0);
}



/** End of Ray **/

vec3 linear_to_srgb(vec3 linearColor) {
    return mix(
        linearColor * 12.92, 
        pow(linearColor, vec3(1.0 / 2.4)) * 1.055 - 0.055, 
        step(0.0031308, linearColor)
    );
}

// in: “pass by value”; if the parameter’s value is changed in the function, the actual parameter from the calling statement is unchanged.

// out: “pass by reference”; the parameter is not initialized when the function is called; any changes in the parameter’s value changes the actual parameter from the calling statement.

// inout: the parameter’s value is initialized by the calling statement and any changes made by the function change the actual parameter from the calling statement.

//Array of vec4 for sphere center and radius
//Array of materials for spheres which matches with index of the sphere
//vec4 representation for materials
//array of int that reprenset which mats ex: 1=>metal 2=>lambertian 3=>dielectric

void main() {
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);


    uint width = gl_NumWorkGroups.x * gl_WorkGroupSize.x;
    uint height = gl_NumWorkGroups.y * gl_WorkGroupSize.y;

    float imageAspectRatio = float(width) / float(height);

    uint random_state = time * (x + y * 36) + 1;

    // Camera settings
    float focal_length = 1.0;
    float viewport_height = 2.0;
    float viewport_width = float(width) / float(height) * viewport_height;
    vec3 camera_center = vec3(0.0, 0.0, 0.0); 

    // Viewport basis vectors
    vec3 viewport_u = vec3(viewport_width, 0, 0);
    vec3 viewport_v = vec3(0, viewport_height, 0); 

    vec3 pixel_delta_u = viewport_u / float(width);
    vec3 pixel_delta_v = viewport_v / float(height);

    // Calculate pixel position
    vec3 viewport_upper_left = camera_center - vec3(0, 0, focal_length) - viewport_u / 2 - viewport_v / 2;
    vec3 pixel_center = viewport_upper_left + float(x) * pixel_delta_u + float(y) * pixel_delta_v;
    vec3 pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

    // Construct the ray
    // Ray ray;
    // ray.origin = camera_center;
    // ray.direction = normalize(pixel_center - camera_center);

    uint samples_per_pixels = 8;
    uint max_bounces = 8;

    vec3 pixel_color = vec3(0.0, 0.0, 0.0);
    for (int s = 0; s < samples_per_pixels; s++){
            vec3 offset = sample_square(random_state);
            vec3 pixel_sample = pixel00_loc + ((x + offset.x) * pixel_delta_u)+ ((y + offset.y) * pixel_delta_v);

        Ray ray;
        ray.origin = camera_center;
        ray.direction = normalize(pixel_sample - camera_center);
        pixel_color += ray_color(ray, max_bounces, random_state);
    }
    vec3 prevColor = imageLoad(srcImage, pixel_coords).xyz;

    float scale_factor = 1.0 / float(samples_per_pixels);
    pixel_color *= scale_factor;
    pixel_color = linear_to_srgb(pixel_color);

    vec3 sum_color = prevColor * max(frameIndex, 1);

    vec3 final_color = (pixel_color + sum_color) / float(frameIndex + 1);
    imageStore(destImage, pixel_coords, vec4(final_color, 1.0));
}

