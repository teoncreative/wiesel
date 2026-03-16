#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT float visibility;

void main() {
  // Hit = shadowed
  visibility = 0.0;
}