/*
* This is embree's minimal.cpp with a ray trace for each pixel and an
* output to a ppm folder with conversion to png with imagemagik
*/


#include <embree4/rtcore.h>
#include <stdio.h>
#include <math.h>
#include <limits>
#include <stdio.h>

#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>

#if defined(_WIN32)
#  include <conio.h>
#  include <windows.h>
#endif

#if defined(RTC_NAMESPACE_USE)
RTC_NAMESPACE_USE
#endif

/*
 * We will register this error handler with the device in initializeDevice(),
 * so that we are automatically informed on errors.
 * This is extremely helpful for finding bugs in your code, prevents you
 * from having to add explicit error checking to each Embree API call.
 */
    void errorFunction(void* userPtr, enum RTCError error, const char* str)
{
    printf("error %d: %s\n", error, str);
}

/*
 * Embree has a notion of devices, which are entities that can run
 * raytracing kernels.
 * We initialize our device here, and then register the error handler so that
 * we don't miss any errors.
 *
 * rtcNewDevice() takes a configuration string as an argument. See the API docs
 * for more information.
 *
 * Note that RTCDevice is reference-counted.
 */
RTCDevice initializeDevice()
{
    RTCDevice device = rtcNewDevice(NULL); // NULL is default. Note verbose=0 in [0,1,2,3] 

    if (!device) {
        printf("error %d: cannot create device\n", rtcGetDeviceError(NULL));
    }

    rtcSetDeviceErrorFunction(device, errorFunction, NULL);
    return device;
}

/*
 * Create a scene, which is a collection of geometry objects. Scenes are
 * what the intersect / occluded functions work on. You can think of a
 * scene as an acceleration structure, e.g. a bounding-volume hierarchy.
 *
 * Scenes, like devices, are reference-counted.
 */
RTCScene initializeScene(RTCDevice device)
{
    RTCScene scene = rtcNewScene(device);

    /*
     * Create a triangle mesh geometry, and initialize a single triangle.
     * You can look up geometry types in the API documentation to
     * find out which type expects which buffers.
     *
     * We create buffers directly on the device, but you can also use
     * shared buffers. For shared buffers, special care must be taken
     * to ensure proper alignment and padding. This is described in
     * more detail in the API documentation.
     */
    RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
    float* vertices = (float*)rtcSetNewGeometryBuffer(geom,
        RTC_BUFFER_TYPE_VERTEX,
        0,
        RTC_FORMAT_FLOAT3,
        3 * sizeof(float),
        3);

    unsigned* indices = (unsigned*)rtcSetNewGeometryBuffer(geom,
        RTC_BUFFER_TYPE_INDEX,
        0,
        RTC_FORMAT_UINT3,
        3 * sizeof(unsigned),
        1);

    if (vertices && indices)
    {
        vertices[0] = 0.f; vertices[1] = 0.f; vertices[2] = 0.f;
        vertices[3] = 1.f; vertices[4] = 0.f; vertices[5] = 0.f;
        vertices[6] = 0.f; vertices[7] = 1.f; vertices[8] = 0.f;

        indices[0] = 0; indices[1] = 1; indices[2] = 2;
    }

    /*
     * You must commit geometry objects when you are done setting them up,
     * or you will not get any intersections.
     */
    rtcCommitGeometry(geom);

    /*
     * In rtcAttachGeometry(...), the scene takes ownership of the geom
     * by increasing its reference count. This means that we don't have
     * to hold on to the geom handle, and may release it. The geom object
     * will be released automatically when the scene is destroyed.
     *
     * rtcAttachGeometry() returns a geometry ID. We could use this to
     * identify intersected objects later on.
     */
    rtcAttachGeometry(scene, geom);
    rtcReleaseGeometry(geom);

    /*
     * Like geometry objects, scenes must be committed. This lets
     * Embree know that it may start building an acceleration structure.
     */
    rtcCommitScene(scene);

    return scene;
}

// most recent intersection stored globally; probably bad practice
static bool g_lastIntersection = false;

/*
 * Cast a single ray with origin (ox, oy, oz) and direction
 * (dx, dy, dz).
 */
void castRay(RTCScene scene,
    float ox, float oy, float oz,
    float dx, float dy, float dz)
{
    /*
     * The ray hit structure holds both the ray and the hit.
     * The user must initialize it properly -- see API documentation
     * for rtcIntersect1() for details.
     */
    struct RTCRayHit rayhit;
    rayhit.ray.org_x = ox;
    rayhit.ray.org_y = oy;
    rayhit.ray.org_z = oz;
    rayhit.ray.dir_x = dx;
    rayhit.ray.dir_y = dy;
    rayhit.ray.dir_z = dz;
    rayhit.ray.tnear = 0;
    rayhit.ray.tfar = std::numeric_limits<float>::infinity();
    rayhit.ray.mask = -1;
    rayhit.ray.flags = 0;
    rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    rayhit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;

    /*
     * There are multiple variants of rtcIntersect. This one
     * intersects a single ray with the scene.
     */
    rtcIntersect1(scene, &rayhit);

    printf("%f, %f, %f: ", ox, oy, oz);
    if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
    {
        /* Note how geomID and primID identify the geometry we just hit.
         * We could use them here to interpolate geometry information,
         * compute shading, etc.
         * Since there is only a single triangle in this scene, we will
         * get geomID=0 / primID=0 for all hits.
         * There is also instID, used for instancing. See
         * the instancing tutorials for more information */
        printf("Found intersection on geometry %d, primitive %d at tfar=%f\n",
            rayhit.hit.geomID,
            rayhit.hit.primID,
            rayhit.ray.tfar);

        // Store the fact that we got an intersection
        g_lastIntersection = true;
    }
    else
    {
        printf("Did not find any intersection.\n");
        g_lastIntersection = false;
    }
}

void savePPM(const char* filename, int width, int height, const unsigned char* data)
{
    // open an output file stream in binary mode (text mode works for ASCII PPM too)
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        return; // file failed to open; should probably error handle here
    }

    // P3 header indicates an ASCII PPM.B
    // width, height, maximum color value (255) following PPM standards
    outFile << "P3\n"
        << width << " " << height << "\n"
        << 255 << "\n";

    // write each pixel
    for (int i = 0; i < width * height; ++i)
    {
        unsigned char c = data[i];
        outFile << static_cast<int>(c) << " "
            << static_cast<int>(c) << " "
            << static_cast<int>(c) << " ";

        // newline after finishing a row
        if ((i + 1) % width == 0)
        {
            outFile << "\n";
        }
    }

    // ofstream automatically closes as it goes out of scope
}

// boilerplate from embree minimal.cpp
void waitForKeyPressedUnderWindows()
{
#if defined(_WIN32)
    HANDLE hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hStdOutput, &csbi)) {
        printf("GetConsoleScreenBufferInfo failed: %lu\n", GetLastError());
        return;
    }

    /* do not pause when running on a shell */
    if (csbi.dwCursorPosition.X != 0 || csbi.dwCursorPosition.Y != 0)
        return;

    /* only pause if running in separate console window. */
    printf("\n\tPress any key to exit...\n");
    _getch();
#endif
}

void renderImage(RTCScene scene)
{
    const int width = 256;
    const int height = 256;
    static unsigned char pixels[width * height];

    // for each pixel, cast a ray from z = -1 toward +z
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            // map x,y into [0..1] range
            float fx = (float)x / (width - 1);
            float fy = (float)y / (height - 1);

            // cast a ray: origin = (fx, fy, -1), direction = (0,0,1)
            castRay(scene, fx, fy, -1.f, 0.f, 0.f, 1.f);

            // if intersect, return white; else black
            pixels[y * width + x] = g_lastIntersection ? 255 : 0;
        }
    }

    savePPM("out.ppm", width, height, pixels);
    // convert to png with imagemagick (a tad hacky)
    system("magick convert out.ppm out.png");
}

/* -------------------------------------------------------------------------- */

int main()
{
    /* Initialization. All of this may fail, but we will be notified by
     * our errorFunction. */
    RTCDevice device = initializeDevice();
    RTCScene scene = initializeScene(device);

    // create a 256x256 black/white image based on triangle intersection.
    renderImage(scene);

    // release resources allocated by embree
    rtcReleaseScene(scene);
    rtcReleaseDevice(device);

    /* wait for user input under Windows when opened in separate window */
    waitForKeyPressedUnderWindows();

    std::cout << "Current working directory: "
        << std::filesystem::current_path()
        << std::endl;

    return 0;
}
