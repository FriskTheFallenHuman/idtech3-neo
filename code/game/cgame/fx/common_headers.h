#pragma once
#include "FxSystem.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <list>
#include <map>
#include <string>
#include <vector>
#include "genericparser2.h"
using std::max;
using std::min;

// Packed entity/model/tag identifiers used by the generic attachment scheduler.
#define BOLT_SHIFT 0
#define MODEL_SHIFT 10
#define ENTITY_SHIFT 16
#define BOLT_AND 1023
#define MODEL_AND 63
#define ENTITY_AND 1023
