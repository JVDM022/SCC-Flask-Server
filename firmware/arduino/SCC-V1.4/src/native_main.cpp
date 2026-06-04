#if defined(UNIT_TEST_NATIVE) && !defined(ARDUINO)
int __attribute__((weak)) main() {
  return 0;
}
#endif
