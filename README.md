# simple-galaxy-simulator
a simple milkyway simluator in c/c++

# Macos
``` g++ galaxy.cpp -o galaxy $(pkg-config --cflags --libs gtk4) -lm ```

# Linux
``` gcc `pkg-config --cflags --libs gtk4` -o galaxy galaxy.cpp -lm ```
