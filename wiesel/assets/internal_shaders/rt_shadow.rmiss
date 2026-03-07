#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT float visibility;

void main() {
  // No hit = fully lit
  visibility = 1.0;
}