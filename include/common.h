
#define LOG_MODULE( level, module, log, ... ) printf( #level " (" #module "): " log "\n", ##__VA_ARGS__ );
