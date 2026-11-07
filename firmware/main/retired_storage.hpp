#pragma once
#include "esp_err.h"

namespace el_sistema {
// Migration only: remove private data of retired modules, never whole NVS.
esp_err_t clear_retired_module_storage();
}
