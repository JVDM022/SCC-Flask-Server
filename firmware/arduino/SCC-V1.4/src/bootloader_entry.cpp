#include "bootloader_entry.h"

#include <ctype.h>
#include <string.h>

static const char *skipSpace(const char *s) {
  while (*s == ' ' || *s == '\t') {
    s++;
  }
  return s;
}

static bool commandEquals(const char *line, const char *command) {
  while (*command) {
    if (toupper((unsigned char)*line) != *command) {
      return false;
    }
    line++;
    command++;
  }
  line = skipSpace(line);
  return *line == '\0';
}

bool isBootloaderEntryCommand(const char *line) {
  if (line == NULL) {
    return false;
  }

  line = skipSpace(line);
  return commandEquals(line, "ENTER_BOOTLOADER") ||
         commandEquals(line, "OTA_PREPARE");
}
