#version 460 core

#define MAX_NUM_SPHERES 10

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(rgba32f, location = 0)  readonly uniform image2D srcImage;
layout(rgba32f, location = 1) writeonly uniform image2D destImage;
layout(location = 3) uniform int time;
layout(location = 4) uniform int frameIndex;
layout(location = 5) uniform vec2 imageDimensions;

struct Ray {
    vec3 origin;
    vec3 direction;
};

struct Sphere{
    vec3 position;
    float radius;
    uint material_index;
};


struct Material{
    vec4 color;
    float refractive_index;
};

struct BVHNodeFlat {
    vec4 aabbMin;
    vec4 aabbMax;
    ivec4 meta; // x: left, y: right, z: sphereIndex, w: unused
};

uniform int num_objects;
uniform int bvh_size;
uniform int root_index;

layout(std430, binding = 0) buffer SpheresBuffer{
    Sphere spheres[];
};

layout(std430, binding = 1) buffer MatsBuffer{
    Material mats[];
};

layout(std140, binding = 2) uniform Camera {
    mat4 viewMatrix;
    mat4 projMatrix;
    mat4 invViewMatrix;
    mat4 invProjMatrix;
    vec4 cameraPosition;
    float focus_distance;
    float defocus_angle;
};

layout(std430, binding = 3) buffer BVHBuffer {
    BVHNodeFlat nodes[];
};



const float infinity = 1./0.;
const float PI = 3.1415926535897932384626433832795;

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
    return (dot(direction, normal) > 0.0) ? direction : -direction;
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

bool intersect_aabb(in Ray r, in vec3 min_b, in vec3 max_b, in vec3 inv_dir) {
    vec3 tmin = (min_b - r.origin) * inv_dir;
    vec3 tmax = (max_b - r.origin) * inv_dir;
    
    vec3 t1 = min(tmin, tmax);
    vec3 t2 = max(tmin, tmax);
    
    float t_near = max(max(t1.x, t1.y), t1.z);
    float t_far = min(min(t2.x, t2.y), t2.z);
    
    return t_near < t_far && t_far > 0.0f;
}

bool world_hit_aabb(in Ray r, in float ray_tmin, in float ray_tmax, inout HitRecord hit_rec) {
    vec3 inv_dir = 1.0f / r.direction; 

    HitRecord temp_rec;
    bool hit_anything = false;
    float closest_so_far = ray_tmax;

    // Start with the root node of the BVH
    int stack[1000]; // Stack for BVH traversal, adjust size if needed
    int stack_ptr = 0;
    stack[stack_ptr++] = root_index; // Start at the root node
    
    while (stack_ptr > 0) {
        int node_idx = stack[--stack_ptr];  // Pop a node index from the stack
        BVHNodeFlat node = nodes[node_idx];  // Get the node from the BVH
        
        // Check if the ray intersects the AABB of this node
        if (intersect_aabb(r, node.aabbMin.xyz, node.aabbMax.xyz, inv_dir)) {
            
            // If it's a leaf node, check the sphere
            if (node.meta.z != -1) {  // leaf node condition (sphere index >= 0)
                Sphere s = spheres[node.meta.z];
                if (hit_sphere(r, s, ray_tmin, closest_so_far, temp_rec)) {
                    if (temp_rec.t < closest_so_far) {
                        hit_anything = true;
                        closest_so_far = temp_rec.t;
                        hit_rec = temp_rec;
                    }
                }
            } else { // Internal node, push left and right children to stack
                if (node.meta.x != -1) {
                    stack[stack_ptr++] = node.meta.x;
                }
                if (node.meta.y != -1) {
                    stack[stack_ptr++] = node.meta.y;
                }
            }
        }
    }
    
    return hit_anything;
}

float reflectance(float cosine, float ref_idx) {
    // Schlick's approximation
    cosine = clamp(cosine, 0.0, 1.0);
    float r0 = (1.0 - ref_idx) / (1.0 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow(1.0 - cosine, 5.0);
}

bool scatter(uint state, in Ray ray_in, in HitRecord hit_rec, inout vec3 attenuation, inout Ray scattered) {
    vec3 reflected;
    float fuzz = mats[hit_rec.mat_index].color.a;

    // Lambertian scatter
    if (fuzz == -1.0) {
        reflected = hit_rec.normal + random_unit_vector(state);   
        // Catch degenerate rays
        if(length(reflected) < 0.0001) 
            reflected = hit_rec.normal; 
    }

    // Metal reflect with fuzziness
    else if (fuzz >= 0.0 && fuzz <= 1.0) {
        reflected = reflect(ray_in.direction, hit_rec.normal);
        reflected = normalize(reflected) + fuzz * random_unit_vector(state);
    } 

    // Dielectric refraction
    else {
        attenuation = vec3(1.0, 1.0, 1.0);

        float ri = hit_rec.front_face ? (1.0 / mats[hit_rec.mat_index].refractive_index)
                                    : mats[hit_rec.mat_index].refractive_index;

        vec3 unit_direction = normalize(ray_in.direction);
        vec3 normal = hit_rec.normal;  // ensure normalized

        float cos_theta = clamp(dot(-unit_direction, normal), -1.0, 1.0);
        float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));

        bool cannot_refract = ri * sin_theta > 1.0;

        vec3 direction;
        if (cannot_refract || reflectance(cos_theta, ri) > RandomUnilateral(state)) {
            direction = reflect(unit_direction, normal);
        } else {
            direction = refract(unit_direction, normal, ri);
        }

        // Catch degenerate rays
        if(length(direction) < 0.0001) 
            direction = normal; 

        direction = normalize(direction); 
        scattered = Ray(hit_rec.point, direction);
        return true;
    }

    reflected = normalize(reflected);   
    scattered = Ray(hit_rec.point, reflected);
    attenuation = mats[hit_rec.mat_index].color.rgb;
    return true;
}

// Original ray color
vec3 ray_color(in Ray ray, uint max_bounces, inout uint state) {
    vec3 accumulated_color  = vec3(1.0);
    
    Ray current_ray;
    current_ray.origin = ray.origin;
    current_ray.direction = ray.direction;

    for (int i = 0; i < max_bounces; i++) {
        HitRecord hit_rec;
        if (world_hit_aabb(current_ray, 0.001, infinity, hit_rec)) {
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
    return accumulated_color; // max depth reached
}
bool ray_contributes_to_color(vec3 color, float threshold) {
    return length(color) > threshold;  // Only continue if the color is above the threshold
}

// early ray termination included
vec3 ray_color2(in Ray ray, uint max_bounces, inout uint state) {
    vec3 accumulated_color = vec3(1.0);
    
    Ray current_ray;
    current_ray.origin = ray.origin;
    current_ray.direction = ray.direction;

    for (int bounce = 0; bounce < max_bounces; bounce++) {
        HitRecord hit_rec;
        if (world_hit_aabb(current_ray, 0.001, infinity, hit_rec)) {
            Ray scattered;
            vec3 attenuation;
            if (scatter(state, current_ray, hit_rec, attenuation, scattered)) {
                accumulated_color *= attenuation;
                current_ray = scattered;
                vec3 acc = accumulated_color;

                // Early termination if contribution is below a threshold
                if (!ray_contributes_to_color(accumulated_color, 0.01))
                    break;  // Stop further bounces
            } 
            else {
                break;
            }
        } else {
            vec3 unit_direction = normalize(current_ray.direction);
            float blend = 0.5 * (unit_direction.y + 1.0);
            return accumulated_color * mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), blend);
        }
    }
    return accumulated_color; // Default black if max depth reached
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

float random_float(inout uint state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return float(state & 0x00FFFFFFu) / float(0x01000000);
}

vec2 sample_disk(inout uint state) {
    // Generate two random numbers in [0, 1)
    float u1 = RandomUnilateral(state);
    float u2 = RandomUnilateral(state);

    // Map to [-1, 1]
    float x = 2.0 * u1 - 1.0;
    float y = 2.0 * u2 - 1.0;

    // Handle degenerate case at origin
    if (x == 0.0 && y == 0.0) {
        return vec2(0.0);
    }

    float r, theta;
    if (abs(x) > abs(y)) {
        r = x;
        theta = (PI / 4.0) * (y / x);
    } else {
        r = y;
        theta = (PI / 2.0) - (PI / 4.0) * (x / y);
    }

    return r * vec2(cos(theta), sin(theta));
}


void main() {
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);

    uint width = uint(imageDimensions.x);
    uint height = uint(imageDimensions.y);

    uint random_state = time * (x + y * 36) + 1;

    uint samples_per_pixel = 1;
    uint max_bounces = 8;

    vec2 inv_resolution = 1.0 / vec2(width, height);
    vec3 camera_pos = cameraPosition.xyz;
    vec3 camera_right = vec3(invViewMatrix[0].xyz);
    vec3 camera_up    = vec3(invViewMatrix[1].xyz);
    float lens_radius = tan(radians(defocus_angle * 0.5)) * focus_distance;

    vec3 pixel_color = vec3(0.0);

    for (int s = 0; s < samples_per_pixel; ++s) {
        // Subpixel jitter
        vec2 offset = sample_square(random_state).xy;  
        vec2 uv = (vec2(pixel_coords) + offset) * inv_resolution;
        vec2 ndc = uv * 2.0 - 1.0;

        // World-space direction (view → clip → world)
        vec4 view_pos = invProjMatrix * vec4(ndc, -1.0, 1.0);
        view_pos /= view_pos.w;
        vec4 world_pos = invViewMatrix * view_pos;
        vec3 dir = normalize(world_pos.xyz - camera_pos);

        // Sample lens disk and shift ray origin
        vec2 lens_sample = sample_disk(random_state) * lens_radius;
        vec3 lens_offset = camera_right * lens_sample.x + camera_up * lens_sample.y;
        vec3 origin = camera_pos + lens_offset;

        // Recompute ray direction toward focal point
        vec3 focal_point = camera_pos + dir * focus_distance;
        dir = normalize(focal_point - origin);

        Ray ray;
        ray.origin = origin;
        ray.direction = dir;

        pixel_color += ray_color(ray, max_bounces, random_state);
    }

    vec3 prevColor = imageLoad(srcImage, pixel_coords).xyz;

    float scale_factor = 1.0 / float(samples_per_pixel);
    pixel_color *= scale_factor;
    pixel_color = linear_to_srgb(pixel_color);

    vec3 sum_color = prevColor * max(frameIndex, 1);

    vec3 final_color = (pixel_color + sum_color) / float(frameIndex + 1);
    imageStore(destImage, pixel_coords, vec4(final_color, 1.0));
}

