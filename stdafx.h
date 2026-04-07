#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <atlbase.h>
#include <atlconv.h>
#include <atlstr.h>

#include <shellapi.h>

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <pdal/PointTable.hpp>
#include <pdal/PointView.hpp>
#include <pdal/io/LasReader.hpp>
#include <pdal/Options.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <functional>
#include <stdexcept>
#include <memory>
#include <vector>
#include <bitset>
