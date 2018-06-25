#ifndef __RAY_TRACER_H__
#define __RAY_TRACER_H__

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include "Vector.h"
#include "Camera.h"

class Ray;
class Color;
class Intersection;
class Object;
class Light;
class Material;
class NormalMap;
class BSP;

class RayTracer {
public:
   int width;
   int height;
   int maxReflections;
   int superSamples; // Square root of number of samples to use for each pixel.
   Camera camera;
   double imageScale;
   int depthComplexity;
   double dispersion;
   Material* startingMaterial;
   BSP* bsp;

   std::vector<Object*> objects;
   std::vector<Light*> lights;
   std::map<std::string, Material*> materials;

   RayTracer(int, int, int, int, int);

   ~RayTracer();

   void addObject(Object* object) {
      objects.push_back(object);
   }

   void addLight(Light* light) {
      lights.push_back(light);
   }

   void traceRays(std::string);
   void readScene(std::istream&);
   void readModel(std::string, int size, Vector translate, Material* material);

   Color castRayForPixel(int, int, uint64_t&) const;

private:
   Color castRayAtPoint(const Vector&, uint64_t&) const;
   Color castRay(const Ray&, uint64_t&) const;
   bool isInShadow(const Ray&, double) const;
   Intersection getClosestIntersection(const Ray&) const;
   Color performLighting(const Intersection&, uint64_t&) const;
   Color getAmbientLighting(const Intersection&, const Color&) const;
   Color getDiffuseAndSpecularLighting(const Intersection&, const Color&) const;
   Color getSpecularLighting(const Intersection&, Light*) const;
   Color getReflectiveRefractiveLighting(const Intersection&, uint64_t&) const;
   double getReflectance(const Vector&, const Vector&, double, double) const;
   Vector refractVector(const Vector&, const Vector&, double, double) const;
   Vector reflectVector(Vector, Vector) const;
   Material* readMaterial(std::istream&);
   NormalMap* readNormalMap(std::istream&);
   void addMaterial(std::istream&);
};

#endif
