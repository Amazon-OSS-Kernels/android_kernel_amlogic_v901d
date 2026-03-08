#ifndef AMZN_MULTICONFIGS
#define AMZN_MULTICONFIGS

void update_tvconfig(const char *hwid);
void tvconfig_mmc_flash_write(const char *cmd, void *download_buffer, unsigned int download_bytes);
#endif

