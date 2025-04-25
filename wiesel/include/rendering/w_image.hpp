//
// Created by Metehan Gezer on 25/04/2025.
//

#ifndef WIESEL_IMAGE_H
#define WIESEL_IMAGE_H

#include "util/w_utils.hpp"
#include "w_pch.hpp"

namespace Wiesel {
class Image {
 public:
  Image();
  ~Image();

  VkImage m_Image;
};
}
#endif  //WIESEL_IMAGE_H
