#include "echonet.h"
#include "echonet_internal.h"

EchonetConfig *_config = NULL;

void _el_set_config(EchonetConfig *config)
{
    _config = config;
}

EchonetConfig *_el_get_config()
{
    return _config;
}
